#include "org/minima/system/network/minima/n_i_o_server.hpp"

#include <chrono>
#include <thread>
#include <system_error>
#include <cstring>
#include <cstdlib> // std::exit, std::atoi

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/message_processor.hpp"

// Assume the project provides NIOClient with the needed interface
#include "org/minima/system/network/minima/n_i_o_client.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX 1
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  // Avoid Windows macro collision with our MessageProcessor::PostMessage
  #ifdef PostMessage
    #undef PostMessage
  #endif
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netinet/tcp.h>
  #include <errno.h>
  #include <netdb.h> // getnameinfo, NI_MAXHOST, NI_MAXSERV
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

// Static member
std::atomic<bool> NIOServer::mTraceON{false};

static inline bool isValidHandle(SocketHandle h) {
#ifdef _WIN32
    return h != static_cast<SocketHandle>(INVALID_SOCKET);
#else
    return h >= 0;
#endif
}

static inline SocketHandle fromNativeSock(
#ifdef _WIN32
    SOCKET s
#else
    int s
#endif
) {
    return static_cast<SocketHandle>(s);
}

#ifdef _WIN32
static inline SOCKET toNativeSock(SocketHandle h) { return static_cast<SOCKET>(h); }
#else
static inline int toNativeSock(SocketHandle h) { return static_cast<int>(h); }
#endif

NIOServer::NIOServer(int zPort, org::minima::utils::messages::MessageProcessor& zNIOManager)
    : mNIOManager(zNIOManager)
    , mPort(zPort)
    , mShutDown(false)
    , mIsRunning(false)
    , mServerSocket(static_cast<SocketHandle>(-1))
    , mWakeupRead(static_cast<SocketHandle>(-1))
    , mWakeupWrite(static_cast<SocketHandle>(-1)) {
    createWakeupPipe();
}

NIOServer::~NIOServer() {
    // Ensure shutdown is called
    if (!mShutDown.load()) {
        shutdown();
    }
    
    // Wait for thread to finish (with timeout)
    if (mServerThread.joinable()) {
        // Give it a reasonable time to shut down
        auto start = std::chrono::steady_clock::now();
        while (mIsRunning.load() && 
               std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        mServerThread.join();
    }
    
    // Ensure server socket is closed
    if (isValidHandle(mServerSocket)) {
        closeSocket(mServerSocket);
        mServerSocket = static_cast<SocketHandle>(-1);
    }

    // Close the wakeup pipe
    if (isValidHandle(mWakeupRead)) {
        closeSocket(mWakeupRead);
        mWakeupRead = static_cast<SocketHandle>(-1);
    }
    if (isValidHandle(mWakeupWrite)) {
        closeSocket(mWakeupWrite);
        mWakeupWrite = static_cast<SocketHandle>(-1);
    }

    // Disconnect all clients on destruction
    std::lock_guard<std::mutex> lock(mClientsMutex);
    for (auto& kv : mClients) {
        if (kv.second) {
            try { kv.second->disconnect(); } catch (...) {}
        }
    }
    mClients.clear();
}

void NIOServer::start() {
    if (mServerThread.joinable()) {
        // Already started
        org::minima::utils::MinimaLogger::log("NIOServer already started");
        return;
    }
    
    // org::minima::utils::MinimaLogger::log("Starting NIOServer thread on port " + std::to_string(mPort));
    
    // Start the server thread
    mServerThread = std::thread(&NIOServer::run, this);
}

void NIOServer::shutdown() {
    mShutDown = true;
    // Wake the select loop so it notices shutdown promptly.
    wakeupSelect();
}

int NIOServer::getNetClientSize() const {
    std::lock_guard<std::mutex> lock(mClientsMutex);
    return static_cast<int>(mClients.size());
}

bool NIOServer::isRunning() const {
    return mIsRunning.load();
}

std::vector<std::shared_ptr<NIOClient>> NIOServer::getAllNIOClients() {
    std::vector<std::shared_ptr<NIOClient>> res;
    std::lock_guard<std::mutex> lock(mClientsMutex);
    res.reserve(mClients.size());
    for (auto& kv : mClients) {
        if (kv.second) res.push_back(kv.second);
    }
    return res;
}

std::shared_ptr<NIOClient> NIOServer::getClient(const std::string& zUID) {
    std::lock_guard<std::mutex> lock(mClientsMutex);
    auto it = mClients.find(zUID);
    if (it != mClients.end()) {
        return it->second;
    }
    return nullptr;
}

void NIOServer::regsiterNewSocket(SocketHandle zSocket) {
    std::lock_guard<std::mutex> lock(mRegisterMutex);
    mRegisterChannels.push_back(zSocket);
    // Wake select() so the new socket is added and readied immediately.
    wakeupSelect();
}

void NIOServer::disconnect(const std::string& zUID) {
    std::lock_guard<std::mutex> lock(mDisconnectMutex);
    mDisconnectChannels.push_back(zUID);
    // No selector to wake; the run loop polls at short intervals
}

void NIOServer::sendMessage(const std::string& zUID, const org::minima::objects::base::MiniData& zData) {
    std::shared_ptr<NIOClient> client = getClient(zUID);
    if (client) {
        try {
            client->sendData(zData);
            // Wake select() so it starts write-monitoring this client immediately.
            wakeupSelect();
        } catch (const std::exception& exc) {
            org::minima::utils::MinimaLogger::log(exc);
        } catch (...) {
            org::minima::utils::MinimaLogger::log(std::string("Unknown exception in sendMessage"));
        }
    }
}

void NIOServer::sendMessageAll(const org::minima::objects::base::MiniData& zData) {
    std::vector<std::shared_ptr<NIOClient>> snapshot;
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        for (auto& kv : mClients) {
            if (kv.second) snapshot.push_back(kv.second);
        }
    }
    bool any = false;
    for (auto& nioc : snapshot) {
        if (nioc) {
            if (nioc->isValidGreeting()) {
                try {
                    nioc->sendData(zData);
                    any = true;
                } catch (const std::exception& exc) {
                    org::minima::utils::MinimaLogger::log(exc);
                } catch (...) {
                    org::minima::utils::MinimaLogger::log(std::string("Unknown exception in sendMessageAll"));
                }
            }
        }
    }
    if (any) {
        // Wake select() so it starts write-monitoring clients immediately.
        wakeupSelect();
    }
}

void NIOServer::run() {
    try {
        mIsRunning = true;

#ifdef _WIN32
        // Ensure Winsock is initialized
        static std::atomic<bool> wsInit{false};
        static std::mutex wsMutex;
        if (!wsInit.load()) {
            std::lock_guard<std::mutex> lk(wsMutex);
            if (!wsInit.load()) {
                WSADATA wsaData;
                int wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
                if (wsaerr != 0) {
                    org::minima::utils::MinimaLogger::log("[!] NIO ERROR - WSAStartup failed");
                    mIsRunning = false;
                    return;
                }
                wsInit = true;
            }
        }
#endif

        // Create, bind, and listen on the server socket
        mServerSocket = createAndBindServer(mPort);
        if (!isValidHandle(mServerSocket)) {
            // Fatal condition already logged; match Java’s behavior to halt (clean exit here)
            std::exit(0);
            return;
        }

        // Notify start
        auto msg = std::make_shared<org::minima::utils::messages::Message>("NIO_SERVERSTARTED");
        mNIOManager.PostMessage(msg);

        // Main loop: event-driven with select() instead of Java NIO Selector.
        while (!mShutDown.load()) {
            // Drain pending registrations and disconnects before each select so that
            // we rebuild an accurate fd set.
            {
                std::vector<SocketHandle> toadd;
                {
                    std::lock_guard<std::mutex> lock(mRegisterMutex);
                    toadd.swap(mRegisterChannels);
                }
                for (auto sock : toadd) {
                    try {
                        addChannel(false, sock);
                    } catch (const std::exception& exc) {
                        org::minima::utils::MinimaLogger::log(exc);
                        closeSocket(sock);
                    } catch (...) {
                        closeSocket(sock);
                    }
                }
            }

            {
                std::vector<std::string> todc;
                {
                    std::lock_guard<std::mutex> lock(mDisconnectMutex);
                    todc.swap(mDisconnectChannels);
                }
                for (const auto& uid : todc) {
                    std::shared_ptr<NIOClient> client;
                    {
                        std::lock_guard<std::mutex> lock(mClientsMutex);
                        auto it = mClients.find(uid);
                        if (it != mClients.end()) {
                            client = it->second;
                            mClients.erase(it);
                        }
                    }
                    if (client) {
                        try { client->disconnect(); } catch (...) {}
                        auto msg = std::make_shared<org::minima::utils::messages::Message>("NIO_DISCONNECTED");
                        msg->addObject("client", client).addBoolean("reconnect", false);
                        mNIOManager.PostMessage(msg);
                    }
                }
            }

            // Build the fd set for select().
            fd_set readfds;
            fd_set writefds;
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);

            // Guard every FD_SET against FD_SETSIZE overflow (defensive, no redesign).
            SocketHandle maxfd = 0;
            {
                int sfd = toNativeSock(mServerSocket);
                if (sfd >= 0 && sfd < FD_SETSIZE) {
                    FD_SET(sfd, &readfds);
                    maxfd = mServerSocket;
                }
            }

            // Include the wakeup pipe read side so select() returns when woken.
            if (isValidHandle(mWakeupRead)) {
                int wfd = toNativeSock(mWakeupRead);
                if (wfd >= 0 && wfd < FD_SETSIZE) {
                    FD_SET(wfd, &readfds);
                    if (mWakeupRead > maxfd) {
                        maxfd = mWakeupRead;
                    }
                }
            }

            std::vector<std::shared_ptr<NIOClient>> clientsSnapshot;
            {
                std::lock_guard<std::mutex> lock(mClientsMutex);
                clientsSnapshot.reserve(mClients.size());
                for (auto& kv : mClients) {
                    if (kv.second) {
                        clientsSnapshot.push_back(kv.second);
                    }
                }
            }

            for (auto& client : clientsSnapshot) {
                if (!client) continue;
                SocketHandle h = client->getSocketHandle();
                if (!isValidHandle(h)) continue;
                int nfd = toNativeSock(h);
                // Never FD_SET a descriptor >= FD_SETSIZE: that writes past the
                // fixed bitset (memory corruption). Skip such clients instead.
                if (nfd < 0 || nfd >= FD_SETSIZE) continue;
                FD_SET(nfd, &readfds);
                if (client->hasPendingWrite()) {
                    FD_SET(nfd, &writefds);
                }
                if (nfd > maxfd) {
                    maxfd = nfd;
                }
            }

            // Block until I/O is ready (max 30 s, matching Java selector timeout).
            timeval tv{};
            tv.tv_sec = 30;
            tv.tv_usec = 0;
            int sel = select(static_cast<int>(toNativeSock(maxfd)) + 1, &readfds, &writefds, nullptr, &tv);
            if (sel < 0) {
                // Interrupted or fatal error; on shutdown the listen socket is closed.
                if (!mShutDown.load()) {
                    org::minima::utils::MinimaLogger::log("[NIOSERVER] select() error");
                }
                continue;
            }

            // Drain any wakeup bytes so the pipe does not stay readable.
            if (isValidHandle(mWakeupRead) && FD_ISSET(toNativeSock(mWakeupRead), &readfds)) {
                drainWakeup();
            }

            // Accept new connections if the server socket is readable.
            if (FD_ISSET(toNativeSock(mServerSocket), &readfds)) {
                acceptNewConnections(mServerSocket);
            }

            // Dispatch read/write only to the clients that are ready.
            for (auto& client : clientsSnapshot) {
                if (!client) continue;
                SocketHandle h = client->getSocketHandle();
                if (!isValidHandle(h)) continue;

                bool readable = FD_ISSET(toNativeSock(h), &readfds);
                bool writable = FD_ISSET(toNativeSock(h), &writefds);
                if (!readable && !writable) continue;

                try {
                    if (readable) {
                        client->handleRead();
                    }
                    if (writable) {
                        client->handleWrite();
                    }
                } catch (const std::exception& e) {
                    try { client->disconnect(); } catch (...) {}
                    {
                        std::lock_guard<std::mutex> lock(mClientsMutex);
                        auto it = mClients.find(client->getUID());
                        if (it != mClients.end()) {
                            mClients.erase(it);
                        }
                    }
                    if (mTraceON.load()) {
                        org::minima::utils::MinimaLogger::log("[NIOSERVER] NIOClient:" + client->getUID() + " exception - total:" + std::to_string(getNetClientSize()), false);
                    }
                    auto diss = std::make_shared<org::minima::utils::messages::Message>("NIO_DISCONNECTED");
                    bool reconnect = !client->isIncoming();
                    diss->addObject("client", client).addBoolean("reconnect", reconnect);
                    mNIOManager.PostMessage(diss);
                } catch (...) {
                    try { client->disconnect(); } catch (...) {}
                    {
                        std::lock_guard<std::mutex> lock(mClientsMutex);
                        auto it = mClients.find(client->getUID());
                        if (it != mClients.end()) {
                            mClients.erase(it);
                        }
                    }
                    auto diss = std::make_shared<org::minima::utils::messages::Message>("NIO_DISCONNECTED");
                    bool reconnect = !client->isIncoming();
                    diss->addObject("client", client).addBoolean("reconnect", reconnect);
                    mNIOManager.PostMessage(diss);
                }
            }
        }

        // Close server
        if (isValidHandle(mServerSocket)) {
            closeSocket(mServerSocket);
            mServerSocket = static_cast<SocketHandle>(-1);
        }

        // Disconnect all clients
        {
            std::lock_guard<std::mutex> lock(mClientsMutex);
            for (auto& kv : mClients) {
                if (kv.second) {
                    try { kv.second->disconnect(); } catch (...) {}
                }
            }
            mClients.clear();
        }
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    } catch (...) {
        org::minima::utils::MinimaLogger::log(std::string("Unknown exception in NIOServer::run"));
    }

    mIsRunning = false;
}

void NIOServer::addChannel(bool zIncoming, SocketHandle zSocket) {
    // Ensure non-blocking and options
    setSocketNonBlocking(zSocket);
    setSocketOptions(zSocket);

    // Determine IP and port
    auto [ipAddress, port] = getPeerAddressAndPort(zSocket);

    // Create client
    std::shared_ptr<NIOClient> nioc = std::make_shared<NIOClient>(zIncoming, ipAddress, port, zSocket);

    // Set up NIOClient to post messages to NIOManager
    nioc->setIncomingMessageType(NIOManager::NIO_INCOMINGMSG);
    nioc->setMessagePoster([this](org::minima::utils::messages::Message& msg) {

        auto msgPtr = std::make_shared<org::minima::utils::messages::Message>(msg);
        mNIOManager.PostMessage(msgPtr);
    });

    // // Set up traffic tracking callbacks
    // nioc->setOnReadBytes([this](int bytes) {
    //     // Add to traffic listener (if you have one)
    //     // mNIOManager.getTrafficListener().addToTotalRead(bytes);
    // });
    
    // nioc->setOnWriteBytes([this](int bytes) {
    //     // Add to traffic listener (if you have one)
    //     // mNIOManager.getTrafficListener().addToTotalWrite(bytes);
    // });

    // Add to map
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mClients[nioc->getUID()] = nioc;
    }

    if (mTraceON.load()) {
        org::minima::utils::MinimaLogger::log("[NIOSERVER] NEW NIOClient:" + nioc->getUID() + " total:" + std::to_string(getNetClientSize()), false);
    }

    // Post about it
    auto newclient = std::make_shared<org::minima::utils::messages::Message>(NIOManager::NIO_NEWCONNECTION);
    newclient->addObject("client", nioc);
    mNIOManager.PostMessage(newclient);
}

void NIOServer::setSocketNonBlocking(SocketHandle sock) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(toNativeSock(sock), FIONBIO, &mode);
#else
    int flags = fcntl(toNativeSock(sock), F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(toNativeSock(sock), F_SETFL, flags | O_NONBLOCK);
#endif
}

void NIOServer::setSocketOptions(SocketHandle sock) {
    // TCP_NODELAY
    {
        int flag = 1;
#ifdef _WIN32
        setsockopt(toNativeSock(sock), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));
#else
        setsockopt(toNativeSock(sock), IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#endif
    }

    // SO_KEEPALIVE
    {
        int flag = 1;
#ifdef _WIN32
        setsockopt(toNativeSock(sock), SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&flag), sizeof(flag));
#else
        setsockopt(toNativeSock(sock), SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
#endif
    }
}

void NIOServer::closeSocket(SocketHandle sock) {
#ifdef _WIN32
    closesocket(toNativeSock(sock));
#else
    close(toNativeSock(sock));
#endif
}

std::pair<std::string, int> NIOServer::getPeerAddressAndPort(SocketHandle sock) {
    std::string ip = "0.0.0.0";
    int port = 0;

    sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    std::memset(&addr, 0, sizeof(addr));
    if (getpeername(toNativeSock(sock), reinterpret_cast<sockaddr*>(&addr), &addrlen) == 0) {
        char host[
#ifdef NI_MAXHOST
            NI_MAXHOST
#else
            1025
#endif
        ] = {0};
        char serv[
#ifdef NI_MAXSERV
            NI_MAXSERV
#else
            33
#endif
        ] = {0};
        int r = getnameinfo(reinterpret_cast<sockaddr*>(&addr), addrlen, host, sizeof(host), serv, sizeof(serv),
                            NI_NUMERICHOST | NI_NUMERICSERV);
        if (r == 0) {
            ip = host;
            port = std::atoi(serv);
        } else {
            // Fallback to inet_ntop for IPv4/IPv6
            if (addr.ss_family == AF_INET) {
                auto* in = reinterpret_cast<sockaddr_in*>(&addr);
                char buf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf));
                ip = buf;
                port = ntohs(in->sin_port);
            }
#ifdef AF_INET6
            else if (addr.ss_family == AF_INET6) {
                auto* in6 = reinterpret_cast<sockaddr_in6*>(&addr);
                char buf[INET6_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET6, &in6->sin6_addr, buf, sizeof(buf));
                ip = buf;
                port = ntohs(in6->sin6_port);
            }
#endif
        }
    }
    return {ip, port};
}

SocketHandle NIOServer::createAndBindServer(int port) {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        org::minima::utils::MinimaLogger::log("[!] NIO ERROR - socket() failed");
        return fromNativeSock(INVALID_SOCKET);
    }

    // Reuse address
    {
        BOOL opt = TRUE;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    addr.sin_port = htons(static_cast<u_short>(port));

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        // Same behavior as Java: log and halt
        org::minima::utils::MinimaLogger::log("[!] NIO ERROR - MAIN PORT " + std::to_string(port) + " ALREADY IN USE. SHUTTING DOWN");
        closesocket(s);
        return fromNativeSock(INVALID_SOCKET);
    }

    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        org::minima::utils::MinimaLogger::log("[!] NIO ERROR - listen() failed");
        closesocket(s);
        return fromNativeSock(INVALID_SOCKET);
    }

    // Non-blocking
    {
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);
    }

    return fromNativeSock(s);
#else
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        org::minima::utils::MinimaLogger::log("[!] NIO ERROR - socket() failed");
        return fromNativeSock(-1);
    }

    // Reuse address
    {
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        org::minima::utils::MinimaLogger::log("[!] NIO ERROR - MAIN PORT " + std::to_string(port) + " ALREADY IN USE. SHUTTING DOWN");
        ::close(s);
        return fromNativeSock(-1);
    }

    if (listen(s, SOMAXCONN) < 0) {
        org::minima::utils::MinimaLogger::log("[!] NIO ERROR - listen() failed");
        ::close(s);
        return fromNativeSock(-1);
    }

    // Non-blocking
    {
        int flags = fcntl(s, F_GETFL, 0);
        if (flags < 0) flags = 0;
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
    }

    return static_cast<SocketHandle>(s);
#endif
}

void NIOServer::acceptNewConnections(SocketHandle serverSock) {
    if (!isValidHandle(serverSock)) return;

    for (;;) {
#ifdef _WIN32
        SOCKET s = accept(toNativeSock(serverSock), nullptr, nullptr);
        if (s == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
            // Other errors: break the loop; they will be retried next iteration
            break;
        }
        SocketHandle cli = fromNativeSock(s);
#else
        int s = ::accept(toNativeSock(serverSock), nullptr, nullptr);
        if (s < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) break;
            break;
        }
        SocketHandle cli = static_cast<SocketHandle>(s);
#endif
        try {
            addChannel(true, cli);
        } catch (...) {
            // On failure, close socket
            closeSocket(cli);
        }
    }
}

bool NIOServer::createWakeupPipe() {
#ifdef _WIN32
    // On Windows use a TCP loopback socketpair as a portable self-pipe.
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listener);
        return false;
    }
    int addrlen = sizeof(addr);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &addrlen) == SOCKET_ERROR) {
        closesocket(listener);
        return false;
    }
    if (listen(listener, 1) == SOCKET_ERROR) {
        closesocket(listener);
        return false;
    }
    SOCKET writer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (writer == INVALID_SOCKET) {
        closesocket(listener);
        return false;
    }
    if (connect(writer, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(writer);
        closesocket(listener);
        return false;
    }
    SOCKET reader = accept(listener, nullptr, nullptr);
    closesocket(listener);
    if (reader == INVALID_SOCKET) {
        closesocket(writer);
        return false;
    }
    u_long mode = 1;
    ioctlsocket(reader, FIONBIO, &mode);
    ioctlsocket(writer, FIONBIO, &mode);
    mWakeupRead  = fromNativeSock(reader);
    mWakeupWrite = fromNativeSock(writer);
    return true;
#else
    int fds[2];
    if (pipe(fds) != 0) return false;
    int flags = fcntl(fds[0], F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(fds[1], F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(fds[1], F_SETFL, flags | O_NONBLOCK);
    mWakeupRead  = fromNativeSock(fds[0]);
    mWakeupWrite = fromNativeSock(fds[1]);
    return true;
#endif
}

void NIOServer::wakeupSelect() {
    if (!isValidHandle(mWakeupWrite)) return;
    char c = 1;
#ifdef _WIN32
    ::send(toNativeSock(mWakeupWrite), &c, 1, 0);
#else
    (void)::write(static_cast<int>(mWakeupWrite), &c, 1);
#endif
}

void NIOServer::drainWakeup() {
    if (!isValidHandle(mWakeupRead)) return;
    char buf[16];
#ifdef _WIN32
    while (::recv(toNativeSock(mWakeupRead), buf, sizeof(buf), 0) > 0) {}
#else
    while (::read(static_cast<int>(mWakeupRead), buf, sizeof(buf)) > 0) {}
#endif
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org