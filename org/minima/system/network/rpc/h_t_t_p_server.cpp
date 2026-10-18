#include "org/minima/system/network/rpc/h_t_t_p_server.hpp"

#include <thread>
#include <string>
#include <sstream>
#include <mutex>

#include "org/minima/utils/minima_logger.hpp"

#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <string.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

using org::minima::utils::MinimaLogger;

#ifdef _WIN32
static std::once_flag g_wsa_once;

static void ensure_winsock_started() {
    std::call_once(g_wsa_once, [](){
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (res != 0) {
            std::ostringstream oss;
            oss << "WSAStartup failed with error: " << res;
            MinimaLogger::log(oss.str());
        }
    });
}
#endif

#ifdef _WIN32
    using OsSocket = SOCKET;
    static constexpr OsSocket OS_INVALID = INVALID_SOCKET;
#else
    using OsSocket = int;
    static constexpr OsSocket OS_INVALID = -1;
#endif

static bool os_socket_is_valid(OsSocket s) {
#ifdef _WIN32
    return s != INVALID_SOCKET;
#else
    return s >= 0;
#endif
}

static void os_socket_close(OsSocket s) {
#ifdef _WIN32
    if (s != INVALID_SOCKET) {
        closesocket(s);
    }
#else
    if (s >= 0) {
        ::close(s);
    }
#endif
}

static void os_socket_close_from_native(HTTPServer::NativeSocket ns) {
#ifdef _WIN32
    os_socket_close(static_cast<OsSocket>(ns));
#else
    os_socket_close(static_cast<OsSocket>(ns));
#endif
}

HTTPServer::HTTPServer(int port)
    : Server(port) {
    // Do NOT auto-start here. The derived class / owner must call start()
    // after construction is complete to avoid running run() on a partially
    // constructed object and to avoid posting notifications before the
    // NotifyManager thread is ready.
}

HTTPServer::HTTPServer(int port, bool autoStart)
    : Server(port) {
    if (autoStart) {
        start();
    }
}

HTTPServer::~HTTPServer() {
    // Ensure resources are released
    shutdown();
}

void HTTPServer::start() {
    // Mimic Java: new Thread(this).start(); without storing a handle; detach
    std::thread([this]() {
        this->run();
    }).detach();
}

void HTTPServer::shutdown() {
    mRunning.store(false, std::memory_order_relaxed);

    // Close listening socket to unblock accept()
    HTTPServer::NativeSocket ns = mServerSocket;
#ifdef _WIN32
    if (ns != static_cast<NativeSocket>(INVALID_SOCKET)) {
        os_socket_close_from_native(ns);
        mServerSocket = static_cast<NativeSocket>(INVALID_SOCKET);
    }
#else
    if (ns >= 0) {
        os_socket_close_from_native(ns);
        mServerSocket = static_cast<NativeSocket>(-1);
    }
#endif
}

void HTTPServer::run() {
#ifdef _WIN32
    ensure_winsock_started();
#endif

    OsSocket listenSock = OS_INVALID;

    try {
        // Create socket
        listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (!os_socket_is_valid(listenSock)) {
#ifdef _WIN32
            int err = WSAGetLastError();
            std::ostringstream oss;
            oss << "Failed to create socket, error: " << err;
            MinimaLogger::log(oss.str());
#else
            std::ostringstream oss;
            oss << "Failed to create socket, errno: " << errno << " (" << strerror(errno) << ")";
            MinimaLogger::log(oss.str());
#endif
            return;
        }

        // Allow address reuse
        int opt = 1;
        setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
                   reinterpret_cast<const char*>(&opt),
#else
                   &opt,
#endif
                   sizeof(opt));

        // Bind to INADDR_ANY on getPort()
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(static_cast<uint16_t>(getPort()));

        if (::bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEADDRINUSE) {
                MinimaLogger::log("Server @ Port " + std::to_string(getPort()) + " already in use!.. restart required..");
            } else {
                std::ostringstream oss;
                oss << "Bind failed on port " << getPort() << ", error: " << err;
                MinimaLogger::log(oss.str());
            }
#else
            if (errno == EADDRINUSE) {
                MinimaLogger::log("Server @ Port " + std::to_string(getPort()) + " already in use!.. restart required..");
            } else {
                std::ostringstream oss;
                oss << "Bind failed on port " << getPort() << ", errno: " << errno << " (" << strerror(errno) << ")";
                MinimaLogger::log(oss.str());
            }
#endif
            os_socket_close(listenSock);
            return;
        }

        if (::listen(listenSock, SOMAXCONN) != 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            std::ostringstream oss;
            oss << "Listen failed on port " << getPort() << ", error: " << err;
            MinimaLogger::log(oss.str());
#else
            std::ostringstream oss;
            oss << "Listen failed on port " << getPort() << ", errno: " << errno << " (" << strerror(errno) << ")";
            MinimaLogger::log(oss.str());
#endif
            os_socket_close(listenSock);
            return;
        }

        // Store the listening socket in a generic form for shutdown()
#ifdef _WIN32
        mServerSocket = static_cast<NativeSocket>(listenSock);
#else
        mServerSocket = static_cast<NativeSocket>(listenSock);
#endif

        MinimaLogger::log("Server started on port : " + std::to_string(getPort()));

        // Accept loop
        while (mRunning.load(std::memory_order_relaxed)) {
            sockaddr_in clientAddr{};
#ifdef _WIN32
            int clen = sizeof(clientAddr);
#else
            socklen_t clen = sizeof(clientAddr);
#endif

            OsSocket clientSock = ::accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &clen);
            if (!os_socket_is_valid(clientSock)) {
                // Accept failed. If shutting down, exit quietly; otherwise log as per Java.
                if (!mRunning.load(std::memory_order_relaxed)) {
                    break;
                }
#ifdef _WIN32
                int err = WSAGetLastError();
                std::ostringstream oss;
                oss << "RPCServer : Socket Shutdown.. " << err;
                MinimaLogger::log(oss.str());
#else
                std::ostringstream oss;
                oss << "RPCServer : Socket Shutdown.. errno " << errno << " (" << strerror(errno) << ")";
                MinimaLogger::log(oss.str());
#endif
                // Continue retrying accept like Java would loop
                continue;
            }

            try {
                // Create handler and run it on a detached thread
                NativeSocket ns = static_cast<NativeSocket>(clientSock);
                auto handler = getSocketHandler(ns);

                std::thread t(std::move(handler));
                // Mimic Java daemon thread
                t.detach();
            } catch (const std::exception& ex) {
                // If handler creation failed, close the client socket and log
                os_socket_close(clientSock);
                MinimaLogger::log(ex);
            } catch (...) {
                os_socket_close(clientSock);
                MinimaLogger::log(std::string("Unknown exception creating socket handler"));
            }
        }
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    } catch (...) {
        MinimaLogger::log(std::string("Unknown exception in HTTPServer::run"));
    }

    // Cleanup
    if (os_socket_is_valid(listenSock)) {
        os_socket_close(listenSock);
    }
#ifdef _WIN32
    mServerSocket = static_cast<NativeSocket>(INVALID_SOCKET);
#else
    mServerSocket = static_cast<NativeSocket>(-1);
#endif
}

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org