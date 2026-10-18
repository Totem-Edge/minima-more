#include "org/minima/system/network/minima/n_i_o_manager.hpp"

#include <sstream>
#include <stdexcept>
#include <thread>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <cstring>
#include <functional>
#include <cerrno>
#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <cstddef>

// Project headers
#include "org/minima/system/network/minima/n_i_o_server.hpp"
#include "org/minima/system/network/minima/n_i_o_client.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/system/network/minima/n_i_o_traffic.hpp"

#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/main.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"

#include "org/minima/objects/greeting.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/system/commands/network/connect.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/system/params/general_params.hpp"

#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/streamable.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/timer_message.hpp"

// Sockets
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <netinet/tcp.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/select.h>
#endif

#ifdef PostMessage
#undef PostMessage
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

using org::minima::utils::MinimaLogger;

// Static init
long long NIOManager::MAX_ARCHIVE_WRITE = static_cast<long long>(1024) * 1024 * 50; // 50MB

// ThreadPool implementation
class NIOManager::ThreadPool {
public:
    explicit ThreadPool(std::size_t threads = 4)
        : mStop(false), mActive(0) {
        for (std::size_t i = 0; i < threads; ++i) {
            mWorkers.emplace_back([this]() { this->workerLoop(); });
        }
    }

    ~ThreadPool() {
        shutdown();
        awaitTermination(8000);
    }

    void execute(std::function<void()> fn) {
        {
            std::unique_lock<std::mutex> lock(mMutex);
            if (mStop) {
                // do not accept new tasks
                return;
            }
            mTasks.push(std::move(fn));
        }
        mCond.notify_one();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStop = true;
        }
        mCond.notify_all();
    }

    void awaitTermination(long long timeout_ms) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        // Wait until queue empty and no active workers
        std::unique_lock<std::mutex> lock(mDoneMutex);
        while ((std::chrono::steady_clock::now() < deadline) && (getQueueSize() > 0 || mActive.load() > 0)) {
            mDoneCond.wait_for(lock, std::chrono::milliseconds(50));
        }
        // Join threads
        for (auto& t : mWorkers) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCond.wait(lock, [this]() { return mStop || !mTasks.empty(); });
                if (mStop && mTasks.empty()) break;
                task = std::move(mTasks.front());
                mTasks.pop();
                mActive.fetch_add(1);
            }
            try {
                task();
            } catch (const std::exception& e) {
                org::minima::utils::MinimaLogger::log(e);
            } catch (...) {
                org::minima::utils::MinimaLogger::log("[NIOMANAGER] Unknown exception in thread pool task");
            }
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mActive.fetch_sub(1);
            }
            mDoneCond.notify_all();
        }
        mDoneCond.notify_all();
    }

    std::size_t getQueueSize() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mTasks.size();
    }

    std::vector<std::thread> mWorkers;
    std::queue<std::function<void()>> mTasks;
    std::mutex mMutex;
    std::condition_variable mCond;
    bool mStop;
    std::atomic<int> mActive;
    std::mutex mDoneMutex;
    std::condition_variable mDoneCond;
};

// Winsock RAII
#ifdef _WIN32
struct WSAInit {
    WSAInit() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WSAInit() {
        WSACleanup();
    }
};
static WSAInit g_wsaInit;
#endif

static int set_nonblocking(int fd, bool nb) {
#ifdef _WIN32
    u_long mode = nb ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (nb) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
#endif
}

static void closesocket_cross(int fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

// Size the P2P message pool with the device: more cores run more inbound
// messages concurrently (helps sync), but keep it bounded so RAM and DB
// contention stay modest. Override with MINIMA_NIO_WORKERS.
static std::size_t nioWorkerCount() {
    if (const char* e = std::getenv("MINIMA_NIO_WORKERS")) {
        long v = std::strtol(e, nullptr, 10);
        if (v > 0) return static_cast<std::size_t>(v);
    }
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(hw), 8);
    if (n < 2) n = 2;
    return n;
}

NIOManager::NIOManager(org::minima::system::network::NetworkManager& zNetManager)
    : MessageProcessor("NIOMANAGER")
    , SYNC_MAX_TIME(static_cast<long long>(1000) * 60 * 60 * 24 * org::minima::system::params::GeneralParams::NUMBER_DAYS_ARCHIVE)
    , mNetworkManager(&zNetManager)
    , mNIOServer(std::make_unique<org::minima::system::network::minima::NIOServer>(org::minima::system::params::GeneralParams::MINIMA_PORT, *this))
    , mTrafficListener(std::make_unique<org::minima::system::network::minima::NIOTraffic>())
    , mThreadPool(std::make_unique<ThreadPool>(nioWorkerCount())) {
    // Start the NIOServer thread (matching Java: Thread nio = new Thread(mNIOServer); nio.start();)
    if (mNIOServer) {
        mNIOServer->start();  // ← ADD THIS LINE
    }

    // Start worker thread only after all derived-class members are initialized.
    startMessageProcessorThread();
}

NIOManager::~NIOManager() = default;
// NIOManager::NIOManager(NIOManager&&) noexcept = delete;
// NIOManager& NIOManager::operator=(NIOManager&&) noexcept = delete;

org::minima::system::network::minima::NIOServer* NIOManager::getNIOServer() {
    return mNIOServer.get();
}

int NIOManager::getNumberOfConnectedClients() {
    return mNIOServer ? mNIOServer->getNetClientSize() : 0;
}

int NIOManager::getNumberOfConnnectingClients() {
    std::lock_guard<std::mutex> lock(mConnectMutex);
    return static_cast<int>(mConnectingClients.size());
}

std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>> NIOManager::getAllConnectionInfo() {
    std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>> connections;

    {
        std::lock_guard<std::mutex> lock(mConnectMutex);
        for (const auto& it : mConnectingClients) {
            auto nc = it.second;
            connections.emplace_back(std::make_unique<org::minima::system::network::minima::NIOClientInfo>(nc.get(), false));
        }
    }

    auto conns = mNIOServer->getAllNIOClients();
    for (auto& conn : conns) {
        connections.emplace_back(std::make_unique<org::minima::system::network::minima::NIOClientInfo>(conn.get(), true));
    }
    return connections;
}

std::vector<org::minima::system::network::minima::NIOClient*> NIOManager::getAllValidConnectedClients() {
    std::vector<org::minima::system::network::minima::NIOClient*> connections;
    auto conns = mNIOServer->getAllNIOClients();
    for (auto& conn : conns) {
        if (conn->isValidGreeting()) {
            connections.push_back(conn.get());
        }
    }
    return connections;
}

std::vector<org::minima::system::network::minima::NIOClient*> NIOManager::getAllValidOutGoingConnectedClients() {
    std::vector<org::minima::system::network::minima::NIOClient*> connections;
    auto conns = mNIOServer->getAllNIOClients();
    for (auto& conn : conns) {
        if (conn->isValidGreeting() && conn->isOutgoing()) {
            connections.push_back(conn.get());
        }
    }
    return connections;
}

org::minima::utils::json::JSONObject NIOManager::getAllConnectedDetails() {
    int incoming = 0;
    int outgoing = 0;
    int total = 0;

    auto conns = mNIOServer->getAllNIOClients();
    for (auto& conn : conns) {
        if (conn->isValidGreeting()) {
            if (conn->isOutgoing()) {
                outgoing++;
            } else {
                incoming++;
            }
            total++;
        }
    }

    org::minima::utils::json::JSONObject ret;
    ret.put("total", total);
    ret.put("incoming", incoming);
    ret.put("outgoing", outgoing);
    return ret;
}

org::minima::system::network::minima::NIOClient* NIOManager::checkConnected(const std::string& zHost, bool zOnlyConnected) {
    if (!zOnlyConnected) {
        std::lock_guard<std::mutex> lock(mConnectMutex);
        for (const auto& it : mConnectingClients) {
            auto nc = it.second;
            if (zHost == nc->getFullAddress()) {
                return nc.get();
            }
        }
    }

    auto conns = mNIOServer->getAllNIOClients();
    for (auto& conn : conns) {
        if (zHost == conn->getFullAddress()) {
            return conn.get();
        }
    }
    return nullptr;
}

org::minima::system::network::minima::NIOClient* NIOManager::getNIOClient(const std::string& zHost) {
    auto conns = mNIOServer->getAllNIOClients();
    for (auto& conn : conns) {
        if (conn->getFullAddress() == zHost) {
            return conn.get();
        }
    }
    return nullptr;
}

org::minima::system::network::minima::NIOClient* NIOManager::getNIOClientFromUID(const std::string& zUID) {
    return org::minima::system::Main::getInstance()->getNIOManager().getNIOServer()->getClient(zUID).get();
}

void NIOManager::hardShutDown() {
    if (mThreadPool) {
        mThreadPool->shutdown();
        mThreadPool->awaitTermination(8000);
    }
    if (mNIOServer) {
        mNIOServer->shutdown();
    }
    stopMessageProcessor();
}

org::minima::system::network::minima::NIOTraffic& NIOManager::getTrafficListener() {
    return *mTrafficListener;
}

void NIOManager::processMessage(org::minima::utils::messages::Message& zMessage) {
    // Don't process after shutdown/restoring except for shutdown message
    if ((org::minima::system::Main::getInstance()->isShuttingDown() || org::minima::system::Main::getInstance()->isRestoring()) &&
        !zMessage.isMessageType(NIO_SHUTDOWN)) {
        return;
    }

    if (zMessage.isMessageType(NIO_SERVERSTARTED)) {
        // Start P2P
        mNetworkManager->getP2PManager().PostMessage(org::minima::system::network::p2p::P2PFunctions::P2P_INIT);

        // Auto connect list
        if (!org::minima::system::params::GeneralParams::CONNECT_LIST.empty()) {
            std::stringstream ss(org::minima::system::params::GeneralParams::CONNECT_LIST);
            std::string host;
            while (std::getline(ss, host, ',')) {
                std::string trimmed = host;
                // trim spaces
                trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
                trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
                auto msg = org::minima::system::commands::network::connect::createConnectMessage(trimmed);
                if (!msg) {
                    org::minima::utils::MinimaLogger::log("ERROR connect host specified incorrectly : " + trimmed);
                } else {
                    org::minima::utils::MinimaLogger::log("Attempting to connect to specified host in 10 seconds: " + trimmed);

                    // Wait 10 seconds and then connect (matches Java warm-up delay)
                    auto timerMsg = std::make_shared<org::minima::utils::messages::TimerMessage>(10000, *msg);
                    PostTimerMessage(timerMsg);
                }
            }
        }

        // Check last message timers
        PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(LASTREAD_CHECKER, NIO_CHECKLASTMSG));

    } else if (zMessage.isMessageType(NIO_SHUTDOWN)) {
        if (mThreadPool) {
            mThreadPool->shutdown();
            mThreadPool->awaitTermination(8000);
        }
        if (mNIOServer) {
            mNIOServer->shutdown();
        }
        org::minima::utils::MinimaLogger::log("Shutdown Networking..");
        while (mNIOServer && mNIOServer->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        stopMessageProcessor();

    } else if (zMessage.isMessageType(NIO_CONNECT)) {
        std::string host = zMessage.getString("host");
        int port = zMessage.getInteger("port");

        // invalid peer check
        if (org::minima::system::network::p2p::P2PFunctions::isInvalidPeer(host + ":" + std::to_string(port))) {
            org::minima::utils::MinimaLogger::log("NIO_CONNECT : Trying to connect to Invalid Peer - disallowed @ " + host + ":" + std::to_string(port));
            return;
        }

        auto nc = std::make_shared<org::minima::system::network::minima::NIOClient>(host, port);
        {
            std::lock_guard<std::mutex> lock(mConnectMutex);
            mConnectingClients[nc->getUID()] = nc;
        }

        auto msg = std::make_shared<org::minima::utils::messages::Message>(NIO_CONNECTATTEMPT);
        msg->addObject("client", nc);
        PostMessage(msg);

    } else if (zMessage.isMessageType(NIO_CONNECTATTEMPT)) {
        auto anyc = zMessage.getObject("client");
        auto nc = std::any_cast<std::shared_ptr<org::minima::system::network::minima::NIOClient>>(anyc);

        {
            std::lock_guard<std::mutex> lock(mConnectMutex);
            if (mConnectingClients.find(nc->getUID()) == mConnectingClients.end()) {
                org::minima::utils::MinimaLogger::log("Connect attempt to " + nc->getFullAddress() + " cancelled.. (removed from connecting clients)");
                return;
            }
        }

        if (org::minima::system::params::GeneralParams::TXBLOCK_NODE) {
            org::minima::utils::MinimaLogger::log("Slave Node Connect attempt to " + nc->getFullAddress());
        }

        // Connect in separate thread
        connectAttempt(nc);

    } else if (zMessage.isMessageType(NIO_RECONNECT)) {
        auto anyc = zMessage.getObject("client");
        auto nc = std::any_cast<std::shared_ptr<org::minima::system::network::minima::NIOClient>>(anyc);

        nc->incrementConnectAttempts();

        bool reconnect = true;
        bool p2pmessagesent = false;

        if (org::minima::system::params::GeneralParams::TXBLOCK_NODE) {
            if (nc->getConnectAttempts() > RECONNECT_ATTEMPTS) {
                org::minima::utils::MinimaLogger::log("INFO : Slave node attempt reconnect.. " + nc->getFullAddress());
                nc->setConnectAttempts(1);
            }
        } else if (!org::minima::system::params::GeneralParams::P2P_ENABLED) {
            if (nc->getConnectAttempts() > RECONNECT_ATTEMPTS) {
                org::minima::utils::MinimaLogger::log("INFO : P2P disabled.. attempt reconnect.. " + nc->getFullAddress() + " use disconnect to stop.");
                nc->setConnectAttempts(1);
            }
        } else {
            if (nc->getConnectAttempts() > RECONNECT_ATTEMPTS) {
                int connected = getNumberOfConnectedClients();
                int connecting = getNumberOfConnnectingClients();
                if (connected > 0 || connecting > 1) {
                    reconnect = false;

                    p2pmessagesent = true;
                    auto newconn = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::network::p2p::P2PFunctions::P2P_NOCONNECT);
                    newconn->addObject("client", nc);
                    newconn->addString("uid", nc->getUID());
                    mNetworkManager->getP2PManager().PostMessage(newconn);

                    org::minima::utils::MinimaLogger::log("INFO : " + nc->getUID() + "@" + nc->getFullAddress() + " connection failed - no more reconnect attempts ");

                    org::minima::system::network::p2p::P2PFunctions::addInvalidPeer(nc->getFullAddress());
                } else {
                    // org::minima::utils::MinimaLogger::log("INFO : " + nc->getUID() + "@" + nc->getFullAddress() + " Resetting reconnect attempts (no other connections) for " + nc->getFullAddress());
                    // nc->setConnectAttempts(1);

                    org::minima::utils::MinimaLogger::log("INFO : " + nc->getUID() + "@" + 
                        nc->getFullAddress() + " All attempts failed - selecting new peer");

                    // Send P2P_NOCONNECT to trigger new peer selection
                    auto newconn = std::make_shared<org::minima::utils::messages::Message>(
                        org::minima::system::network::p2p::P2PFunctions::P2P_NOCONNECT);
                    newconn->addObject("client", nc);
                    newconn->addString("uid", nc->getUID());
                    mNetworkManager->getP2PManager().PostMessage(newconn);

                    // Don't reset counter - let it disconnect
                    p2pmessagesent = true;
                }
            }
        }

        if (reconnect && org::minima::system::network::p2p::P2PFunctions::isInvalidPeer(nc->getFullAddress())) {
            org::minima::utils::MinimaLogger::log("NIO_RECONNECT : Trying to connect to Invalid Peer - disallowed @ " + nc->getFullAddress());
            reconnect = false;
            if (!p2pmessagesent) {
                auto newconn = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::network::p2p::P2PFunctions::P2P_NOCONNECT);
                newconn->addObject("client", nc);
                newconn->addString("uid", nc->getUID());
                mNetworkManager->getP2PManager().PostMessage(newconn);
            }
        }

        if (reconnect) {
            auto tmsg = std::make_shared<org::minima::utils::messages::TimerMessage>(RECONNECT_TIMER, NIO_CONNECTATTEMPT);
            tmsg->addObject("client", nc);
            PostTimerMessage(tmsg);
        } else {
            std::lock_guard<std::mutex> lock(mConnectMutex);
            mConnectingClients.erase(nc->getUID());
        }

    } else if (zMessage.isMessageType(NIO_DISCONNECTALL)) {
        {
            std::lock_guard<std::mutex> lock(mConnectMutex);
            for (const auto& it : mConnectingClients) {
                disconnect(it.first);
            }
        }
        auto conns = mNIOServer->getAllNIOClients();
        for (auto& conn : conns) {
            disconnect(conn->getUID());
        }

    } else if (zMessage.isMessageType(NIO_DISCONNECT)) {
        std::string uid = zMessage.getString("uid");
        {
            std::lock_guard<std::mutex> lock(mConnectMutex);
            mConnectingClients.erase(uid);
        }
        mNIOServer->disconnect(uid);

    } else if (zMessage.isMessageType(NIO_DISCONNECTED)) {
        auto nioc_any = zMessage.getObject("client");
        auto nioc = std::any_cast<std::shared_ptr<org::minima::system::network::minima::NIOClient>>(nioc_any);

        // Remove from last sync lists (static maps in NIOMessage)
        org::minima::system::network::minima::NIOMessage::mlastSyncReq.erase(nioc->getUID());
        org::minima::system::network::minima::NIOMessage::mLastChainSync.erase(nioc->getUID());

        bool reconnect = false;
        if (zMessage.exists("reconnect")) {
            reconnect = zMessage.getBoolean("reconnect");
        }

        if (!nioc->isValidGreeting()) reconnect = false;
        if (nioc->isIncoming()) reconnect = false;

        if (reconnect && org::minima::system::network::p2p::P2PFunctions::isInvalidPeer(nioc->getFullAddress())) {
            org::minima::utils::MinimaLogger::log("NIO_DISCONNECT : Trying to connect to Invalid Peer - disallowed @ " + nioc->getFullAddress());
            reconnect = false;
        }

        if (org::minima::system::params::GeneralParams::TXBLOCK_NODE) {
            if (zMessage.exists("reconnect")) {
                org::minima::utils::MinimaLogger::log("SLAVE NODE disconneced.. from:" + nioc->getUID() + " req:" + std::string(zMessage.getBoolean("reconnect") ? "true" : "false") + " reconnect:" + (reconnect ? "true" : "false") + " validgreeting:" + (nioc->isValidGreeting() ? "true" : "false") + " host:" + nioc->getFullAddress() + " incoming:" + (nioc->isIncoming() ? "true" : "false"));
            } else {
                org::minima::utils::MinimaLogger::log("SLAVE NODE disconneced.. from:" + nioc->getUID() + " reconnect:" + (reconnect ? "true" : "false") + " validgreeting:" + (nioc->isValidGreeting() ? "true" : "false") + " host:" + nioc->getFullAddress() + " incoming:" + (nioc->isIncoming() ? "true" : "false"));
            }
            if (nioc->isOutgoing() && checkConnected(nioc->getFullAddress(), true) == nullptr) {
                if (!reconnect) {
                    org::minima::utils::MinimaLogger::log("FORCE Slave reconnect " + nioc->getUID() + " " + nioc->getFullAddress());
                    reconnect = true;
                }
            }
        }

        if (reconnect && nioc->isOutgoing()) {
            std::string host = nioc->getHost();
            int port = nioc->getPort();
            auto nc = std::make_shared<org::minima::system::network::minima::NIOClient>(host, port);
            {
                std::lock_guard<std::mutex> lock(mConnectMutex);
                mConnectingClients[nc->getUID()] = nc;
            }
            auto tmsg = std::make_shared<org::minima::utils::messages::TimerMessage>(RECONNECT_TIMER, NIO_CONNECTATTEMPT);
            tmsg->addObject("client", nc);
            PostTimerMessage(tmsg);
        }

        auto newconn = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::network::p2p::P2PFunctions::P2P_DISCONNECTED);
        newconn->addObject("nioclient", nioc);
        newconn->addString("uid", nioc->getUID());
        newconn->addBoolean("incoming", nioc->isIncoming());
        newconn->addBoolean("reconnect", reconnect);
        mNetworkManager->getP2PManager().PostMessage(newconn);

    } else if (zMessage.isMessageType(NIO_NEWCONNECTION)) {

        auto nioc_any = zMessage.getObject("client");
        auto nioc = std::any_cast<std::shared_ptr<org::minima::system::network::minima::NIOClient>>(nioc_any);

        {
            std::lock_guard<std::mutex> lock(mConnectMutex);
            mConnectingClients.erase(nioc->getUID());
        }

        if (org::minima::system::network::p2p::P2PFunctions::isInvalidPeer(nioc->getFullAddress())) {
            org::minima::utils::MinimaLogger::log("Disconnecting invalid peer before sending or recieving ANY data..");
            disconnect(nioc->getUID());
            return;
        }

        if (!nioc->isIncoming()) {        
            if (!nioc->haveSentGreeting()) {
                nioc->setSentGreeting(true);
                org::minima::objects::Greeting greet;
                greet.createGreeting();
                sendNetworkMessage(nioc->getUID(), org::minima::objects::base::MiniByte(org::minima::system::network::minima::NIOMessage::MSG_GREETING()), greet);
            }
        }

    } else if (zMessage.isMessageType(NIO_INCOMINGMSG)) {
        std::string uid = zMessage.getString("uid");
        auto anydata = zMessage.getObject("data");
        auto data = std::any_cast<org::minima::objects::base::MiniData>(anydata);

        auto niomsg = std::make_shared<org::minima::system::network::minima::NIOMessage>(uid, data);
        niomsg->setTrace(isTrace(), getTraceFilter());
        if (zMessage.exists("fullhost")) {
            niomsg->setFullAddress(zMessage.getString("fullhost"));
        }

        if (mThreadPool) {
            // 2. The lambda now captures a (copyable) shared_ptr. Call with ->
            mThreadPool->execute([niomsg]() { niomsg->run(); });
        } else {
            // 3. Call with ->
            niomsg->run();
        }

    } else if (zMessage.isMessageType(NIO_TXPOWREQ)) {
        std::string txpowid = zMessage.getString("txpowid");
        std::string clientid = zMessage.getString("client");
        std::string reason = zMessage.getString("reason");

        if (!org::minima::database::MinimaDB::getDB()->getTxPoWDB().exists(txpowid)) {
            org::minima::utils::MinimaLogger::log("INFO : Requesting TxPoW " + txpowid + " from " + clientid + " : " + reason);
            org::minima::objects::base::MiniData md(txpowid);
            sendNetworkMessage(clientid, org::minima::objects::base::MiniByte(org::minima::system::network::minima::NIOMessage::MSG_TXPOWREQ()), md);
        }

    } else if (zMessage.isMessageType(NIO_SYNCTXBLOCK)) {
        if (org::minima::database::MinimaDB::getDB()->getTxPoWTree().getRoot() == nullptr) {
            org::minima::utils::MinimaLogger::log("No TxPoWTree yet.. required for NIO_SYNCTXBLOCK");
            return;
        }

        std::string clientid = zMessage.getString("client");
        org::minima::database::archive::ArchiveManager& arch = org::minima::database::MinimaDB::getDB()->getArchive();

        std::unique_ptr<org::minima::objects::TxBlock> lastblock = arch.loadLastBlock();
        const org::minima::objects::TxPoW* lastpow;
        if (!lastblock) {
            lastpow = &org::minima::database::MinimaDB::getDB()->getTxPoWTree().getRoot()->getTxPoW();
        } else {
            lastpow = &lastblock->getTxPoW();
        }

        if (lastpow->getBlockNumber().isEqual(org::minima::objects::base::MiniNumber::ONE())) {
            return;
        }

        if (!org::minima::system::params::GeneralParams::ARCHIVE) {
            auto timenow = std::chrono::system_clock::now();
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(timenow.time_since_epoch()).count();
            long long maxtime = millis - SYNC_MAX_TIME;
            if (lastpow->getTimeMilli().getAsLong() < maxtime) {
                if (org::minima::system::params::GeneralParams::IBDSYNC_LOGS) {
                    org::minima::utils::MinimaLogger::log("We have enough archive blocks.. lastblock time filtered");
                }
                return;
            }
        }

        if (org::minima::system::params::GeneralParams::IBDSYNC_LOGS) {
            org::minima::utils::MinimaLogger::log(std::string("[+] Request Sync IBD @ ") + lastpow->getBlockNumber().toString());
        }
        sendNetworkMessage(clientid, org::minima::objects::base::MiniByte(org::minima::system::network::minima::NIOMessage::MSG_IBD_REQ()), *lastpow);

    } else if (zMessage.isMessageType(NIO_CHECKLASTMSG)) {
        auto timenow = std::chrono::system_clock::now();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(timenow.time_since_epoch()).count();

        auto conns = mNIOServer->getAllNIOClients();
        for (auto& conn : conns) {
            long long diff = millis - conn->getLastReadTime();
            if (diff > MAX_LASTREAD_CHECKER) {
                org::minima::utils::MinimaLogger::log("INFO : No recent message (10 mins) from "
                    + conn->getUID() + " disconnect/reconnect incoming:" + (conn->isIncoming() ? "true" : "false")
                    + " valid:" + (conn->isValidGreeting() ? "true" : "false") + " host:" + conn->getFullAddress());

                disconnect(conn->getUID());

                if (!conn->isIncoming() && conn->isValidGreeting()) {
                    auto timedconnect = std::make_shared<org::minima::utils::messages::TimerMessage>(5000, NIO_CONNECT);
                    timedconnect->addString("host", conn->getHost());
                    timedconnect->addInteger("port", conn->getPort());
                    PostTimerMessage(timedconnect);
                }
            }
        }

        PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(LASTREAD_CHECKER, NIO_CHECKLASTMSG));

    } else if (zMessage.isMessageType(NIO_HEALTHCHECK)) {
        // Healthcheck code commented in Java; keep behavior no-op per snippet.
    }
}

void NIOManager::connectAttempt(const std::shared_ptr<org::minima::system::network::minima::NIOClient>& zNIOClient) {
    auto connector = [this, zNIOClient]() {
        try {
            // Resolve and connect with 10s timeout
            int sockfd = -1;

            addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            addrinfo* result = nullptr;
            std::string portstr = std::to_string(zNIOClient->getPort());
            if (getaddrinfo(zNIOClient->getHost().c_str(), portstr.c_str(), &hints, &result) != 0) {
                throw std::runtime_error("getaddrinfo failed");
            }

            bool connected = false;
            for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
                sockfd = static_cast<int>(::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol));
                if (sockfd < 0) continue;

                // Non-blocking connect
                set_nonblocking(sockfd, true);
                int res = ::connect(sockfd, rp->ai_addr, static_cast<int>(rp->ai_addrlen));

#ifdef _WIN32
                int err = WSAGetLastError();
                if (res == 0 || err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEINVAL) {
#else
                if (res == 0 || errno == EINPROGRESS) {
#endif
                    fd_set wfds;
                    FD_ZERO(&wfds);
                    FD_SET(sockfd, &wfds);
                    timeval tv{};
                    tv.tv_sec = 10;
                    tv.tv_usec = 0;
                    int sel = select(sockfd + 1, nullptr, &wfds, nullptr, &tv);
                    if (sel > 0 && FD_ISSET(sockfd, &wfds)) {
                        // Check for errors
                        int so_error = 0;
#ifdef _WIN32
                        int len = sizeof(so_error);
#else
                        socklen_t len = sizeof(so_error);
#endif
                        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
                        if (so_error == 0) {
                            connected = true;

                            // Set TCP socket options
                            int flag = 1;
                            setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
                            int keepalive = 1;
                            setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, sizeof(keepalive));
                            int reuse = 1;
                            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
                            int bufsize = 65536;
                            setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, sizeof(bufsize));
                            setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize));
                        }
                    }
                }
                // Set back to blocking
                set_nonblocking(sockfd, false);

                if (connected) break;
                closesocket_cross(sockfd);
                sockfd = -1;
            }

            if (result) freeaddrinfo(result);

            if (!connected || sockfd < 0) {
                throw std::runtime_error("connect timeout/failure");
            }

            {
                std::lock_guard<std::mutex> lock(mConnectMutex);
                mConnectingClients.erase(zNIOClient->getUID());
            }

            mNIOServer->regsiterNewSocket(sockfd);

            org::minima::utils::MinimaLogger::log("Connected attempt success to " + zNIOClient->getFullAddress());

        } catch (const std::exception& exc) {
            org::minima::utils::MinimaLogger::log(zNIOClient->getUID() + " INFO : connecting attempt "
                + std::to_string(zNIOClient->getConnectAttempts()) + " to " + zNIOClient->getHost() + ":" + std::to_string(zNIOClient->getPort()) + " " + std::string(exc.what()));

            auto reconn = std::make_shared<org::minima::utils::messages::Message>(NIO_RECONNECT);
            reconn->addObject("client", zNIOClient);
            PostMessage(reconn);
        }
    };

    std::thread tt(connector);
    tt.detach();
}

void NIOManager::disconnect(const std::string& zClientUID) {
    disconnect(zClientUID, false);
}

void NIOManager::disconnect(const std::string& zClientUID, bool zRemoveP2P) {
    if (org::minima::system::params::GeneralParams::TXBLOCK_NODE) {
        try {
            throw std::runtime_error("Show Disconnect Stack Trace");
        } catch (const std::exception& exc) {
            org::minima::utils::MinimaLogger::log(exc);
        }
    }

    if (zRemoveP2P && org::minima::system::params::GeneralParams::P2P_ENABLED) {
        auto nioc = getNIOClientFromUID(zClientUID);
        if (nioc != nullptr) {
            org::minima::utils::MinimaLogger::log("Disconnecting and Removing PEER from P2P " + nioc->getFullAddress());
            org::minima::system::network::p2p::P2PFunctions::addInvalidPeer(nioc->getFullAddress());

            // Build inet socket address object (project-specific)
            org::minima::system::network::p2p::messages::InetSocketAddress addr(nioc->getHost(), nioc->getPort());

            auto msg = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::network::p2p::P2PManager::P2P_REMOVE_PEER);
            msg->addObject(org::minima::system::network::p2p::P2PManager::ADDRESS_LITERAL, addr);
            org::minima::system::Main::getInstance()->getNetworkManager().getP2PManager().PostMessage(msg);
        }
    }

    auto msg = std::make_shared<org::minima::utils::messages::Message>(NIOManager::NIO_DISCONNECT);
    msg->addString("uid", zClientUID);
    PostMessage(msg);
}

void NIOManager::sendNetworkMessageAll(const org::minima::objects::base::MiniByte& zType,
                                       org::minima::utils::Streamable& zObject) {
    sendNetworkMessage("", zType, zObject);
}

void NIOManager::sendNetworkMessage(const std::string& zUID,
                                    const org::minima::objects::base::MiniByte& zType,
                                    const org::minima::utils::Streamable& zObject) {
    // Create the network message
    org::minima::objects::base::MiniData niodata = createNIOMessage(zType, zObject);

    if (org::minima::system::params::GeneralParams::NETWORKING_LOGS) {
        org::minima::utils::MinimaLogger::log(std::string("[NETLOGS SEND] to:") + zUID + " type:" + 
            std::to_string(zType.getValue()) +
            " size:" + org::minima::utils::MiniFormat::formatSize(niodata.getLength()));
    }

    std::string howmany = "single";
    if (!zUID.empty()) {
        org::minima::system::Main::getInstance()->getNIOManager().getNIOServer()->sendMessage(zUID, niodata);
    } else {
        org::minima::system::Main::getInstance()->getNIOManager().getNIOServer()->sendMessageAll(niodata);
        howmany = "all";
    }

    try {
        std::string strtype = std::to_string(zType.getValue());
        int size = niodata.getLength();
        auto& traffic = org::minima::system::Main::getInstance()->getNIOManager().getTrafficListener();
        traffic.addWriteBytes(strtype + "_" + howmany, size);
    } catch (...) {
        // ignore
    }
}

org::minima::objects::base::MiniData NIOManager::createNIOMessage(const org::minima::objects::base::MiniByte& zType,
                                                                  const org::minima::utils::Streamable& zObject) {
    try {
        std::ostringstream baos(std::ios::binary);
        // Write type
        const_cast<org::minima::objects::base::MiniByte&>(zType).writeDataStream(baos);
        // Write object
        const_cast<org::minima::utils::Streamable&>(zObject).writeDataStream(baos);
        baos.flush();

        std::string s = baos.str();
        std::vector<std::uint8_t> bb(s.begin(), s.end());
        org::minima::objects::base::MiniData data(bb);
        return data;

    } catch (const std::bad_alloc&) {
        // Attempt to log, then throw
        try {
            org::minima::utils::MinimaLogger::log("OUT OF MEMORY.. on create NIOMsssage:" +
                std::to_string(zType.getValue()));
        } catch (...) {
            // ignore logging exceptions
        }
        throw std::runtime_error("Out Of Memory..");
    }
}

std::shared_ptr<org::minima::objects::Greeting> NIOManager::sendPingMessage(const std::string& zHost, int zPort, bool suppressErrorMessage) {
    std::shared_ptr<org::minima::objects::Greeting> greet;

    auto closeSock = [](int& fd) {
        if (fd >= 0) {
            closesocket_cross(fd);
            fd = -1;
        }
    };

    try {
        // Create the SINGLE_PING message (type + zero-length body)
        auto msgdata = NIOManager::createNIOMessage(
            org::minima::objects::base::MiniByte(org::minima::system::network::minima::NIOMessage::MSG_SINGLE_PING()),
            const_cast<org::minima::objects::base::MiniData&>(org::minima::objects::base::MiniData::ZERO_TXPOWID()));

        // Resolve
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        std::string portstr = std::to_string(zPort);
        if (getaddrinfo(zHost.c_str(), portstr.c_str(), &hints, &result) != 0 || !result) {
            throw std::runtime_error("getaddrinfo failed for " + zHost + ":" + portstr);
        }

        int sockfd = -1;
        bool connected = false;

        for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
            sockfd = static_cast<int>(::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol));
            if (sockfd < 0) continue;

            // Set non-blocking for connect with timeout
            set_nonblocking(sockfd, true);
            int res = ::connect(sockfd, rp->ai_addr, static_cast<int>(rp->ai_addrlen));

            bool inprogress = false;
#ifdef _WIN32
            int werr = WSAGetLastError();
            inprogress = (res == 0) || (werr == WSAEWOULDBLOCK) || (werr == WSAEINPROGRESS) || (werr == WSAEINVAL);
#else
            inprogress = (res == 0) || (errno == EINPROGRESS);
#endif
            if (inprogress) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(sockfd, &wfds);
                timeval tv{};
                tv.tv_sec = 10;
                tv.tv_usec = 0;
                int sel = select(sockfd + 1, nullptr, &wfds, nullptr, &tv);
                if (sel > 0 && FD_ISSET(sockfd, &wfds)) {
                    int so_error = 0;
#ifdef _WIN32
                    int len = sizeof(so_error);
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
#else
                    socklen_t len = sizeof(so_error);
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
#endif
                    if (so_error == 0) {
                        connected = true;
                    }
                }
            } else if (res == 0) {
                connected = true;
            }

            set_nonblocking(sockfd, false);

            if (connected) {
                break;
            }
            closeSock(sockfd);
        }

        if (result) freeaddrinfo(result);

        if (!connected || sockfd < 0) {
            throw std::runtime_error("connect timeout/failure to " + zHost + ":" + portstr);
        }

        // Set recv/send timeouts (10s)
#ifdef _WIN32
        DWORD to = 10000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&to, sizeof(to));
#else
        timeval tv{};
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

        // Send the raw MiniData bytes exactly as Java does (MiniData already frames itself)
        const std::vector<std::uint8_t>& outbuf = msgdata.getBytes();
        const char* buf = reinterpret_cast<const char*>(outbuf.data());
        std::size_t remaining = outbuf.size();
        while (remaining > 0) {
#ifdef _WIN32
            int sent = ::send(sockfd, buf, static_cast<int>(remaining), 0);
#else
            ssize_t sent = ::send(sockfd, buf, remaining, 0);
#endif
            if (sent <= 0) {
                closeSock(sockfd);
                throw std::runtime_error("send failed to " + zHost);
            }
            buf += sent;
            remaining -= static_cast<std::size_t>(sent);
        }

        org::minima::system::Main::getInstance()->getNIOManager().getTrafficListener().addWriteBytes("sendPingMessage", msgdata.getLength());

        // Read response as a single MiniData object (matches Java MiniData.ReadFromStream(dis))
        std::vector<std::uint8_t> payload;
        {
            std::uint8_t lenbuf[4] = {0};
            std::size_t got = 0;
            while (got < 4) {
#ifdef _WIN32
                int r = ::recv(sockfd, (char*)lenbuf + got, static_cast<int>(4 - got), 0);
#else
                ssize_t r = ::recv(sockfd, lenbuf + got, 4 - got, 0);
#endif
                if (r <= 0) {
                    closeSock(sockfd);
                    throw std::runtime_error("recv length failed from " + zHost);
                }
                got += static_cast<std::size_t>(r);
            }

            int mlen = (static_cast<int>(lenbuf[0]) << 24) |
                       (static_cast<int>(lenbuf[1]) << 16) |
                       (static_cast<int>(lenbuf[2]) << 8)  |
                       (static_cast<int>(lenbuf[3]));
            if (mlen < 0 || mlen > org::minima::objects::base::MiniData::MINIMA_MAX_MINIDATA_LENGTH) {
                closeSock(sockfd);
                throw std::runtime_error("invalid ping response length from " + zHost);
            }

            payload.resize(static_cast<std::size_t>(mlen));
            std::size_t recvd = 0;
            while (recvd < payload.size()) {
#ifdef _WIN32
                int r = ::recv(sockfd, (char*)payload.data() + recvd, static_cast<int>(payload.size() - recvd), 0);
#else
                ssize_t r = ::recv(sockfd, payload.data() + recvd, payload.size() - recvd, 0);
#endif
                if (r <= 0) {
                    closeSock(sockfd);
                    throw std::runtime_error("recv payload failed from " + zHost);
                }
                recvd += static_cast<std::size_t>(r);
            }
        }

        closeSock(sockfd);

        org::minima::system::Main::getInstance()->getNIOManager().getTrafficListener().addReadBytes("sendPingMessage", static_cast<int>(payload.size()));

        // Parse the response as a MiniData-wrapped payload: outer length is consumed above,
        // the remaining bytes are a MiniData object whose first byte is the message type.
        std::string respstr(reinterpret_cast<const char*>(payload.data()), payload.size());
        std::istringstream bais(respstr, std::ios::binary);

        org::minima::objects::base::MiniByte rtype(0xFF);
        try {
            rtype = org::minima::objects::base::MiniByte::ReadFromStream(bais);
        } catch (...) {}

        const auto& singlePong = org::minima::system::network::minima::NIOMessage::MSG_SINGLE_PONG();
        const auto& singlePing = org::minima::system::network::minima::NIOMessage::MSG_SINGLE_PING();

        if (rtype.isEqual(singlePong)) {
            try {
                greet = org::minima::objects::Greeting::ReadFromStream(bais);
            } catch (const std::exception& ge) {
                if (!suppressErrorMessage) {
                    org::minima::utils::MinimaLogger::log(std::string("sendPingMessage: Greeting parse failed: ") + ge.what());
                }
                greet.reset();
            }
        } else if (rtype.isEqual(singlePing)) {
            // Some nodes might echo or misbehave; try to parse rest as Greeting anyway
            try {
                greet = org::minima::objects::Greeting::ReadFromStream(bais);
            } catch (...) {
                greet.reset();
            }
            if (!suppressErrorMessage) {
                org::minima::utils::MinimaLogger::log("sendPingMessage: peer answered with SINGLE_PING instead of PONG");
            }
        } else {
            // Unknown type byte. Try parsing the entire payload (without the type we just consumed) as a bare Greeting.
            if (!suppressErrorMessage) {
                org::minima::utils::MinimaLogger::log(std::string("sendPingMessage: unexpected reply type ")
                    + std::to_string(rtype.getValue()) + " from " + zHost + " (trying fallback parse)");
            }
            try {
                // Rewind: parse from start of payload as if it were a bare Greeting stream
                std::istringstream bais2(respstr, std::ios::binary);
                greet = org::minima::objects::Greeting::ReadFromStream(bais2);
            } catch (...) {
                greet.reset();
            }
        }

    } catch (const std::exception& exc) {
        greet.reset();
        if (!suppressErrorMessage) {
            org::minima::utils::MinimaLogger::log(std::string("Error sending Single Ping message to ")
                + zHost + ":" + std::to_string(zPort) + " : " + exc.what());
        }
    }

    return greet;
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org