#include "org/minima/system/network/network_manager.hpp"

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>
#include <exception>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
    #ifdef PostMessage
    #undef PostMessage
    #endif
#else
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/types.h>
    #include <sys/socket.h>
#endif

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_traffic.hpp"

#include "org/minima/system/network/p2p/p2_p_functions.hpp"
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/system/network/p2p2/p2_p2_manager.hpp"

#include "org/minima/system/network/rpc/server.hpp"
#include "org/minima/system/network/rpc/h_t_t_p_server.hpp"
#include "org/minima/system/network/rpc/h_t_t_p_s_server.hpp"
#include "org/minima/system/network/rpc/c_m_d_handler.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"

#include "org/minima/utils/messages/message_processor.hpp"
#include "org/minima/utils/messages/message.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {

namespace {
    // Dummy processor used when P2P/P2P2 is disabled, mirrors Java anonymous class
    class DummyProcessor final : public org::minima::utils::messages::MessageProcessor {
    public:
        explicit DummyProcessor(const std::string& name)
            : org::minima::utils::messages::MessageProcessor(name) {
        startMessageProcessorThread();
    }

    protected:
        void processMessage(org::minima::utils::messages::Message& zMessage) override {
            using org::minima::system::network::p2p::P2PFunctions;
            if (zMessage.isMessageType(P2PFunctions::P2P_SHUTDOWN)) {
                stopMessageProcessor();
            }
        }
    };

    // HTTP server wrapper that creates a CMDHandler for each accepted socket
    class RPC_HTTP_Server final : public org::minima::system::network::rpc::HTTPServer {
    public:
        explicit RPC_HTTP_Server(int port) : HTTPServer(port) {}
        std::function<void()> getSocketHandler(NativeSocket clientSocket) override {
            return [clientSocket]() {
                try {
                    org::minima::system::network::rpc::CMDHandler handler(static_cast<std::intptr_t>(clientSocket));
                    handler.run();
                } catch (const std::exception& ex) {
                    org::minima::utils::MinimaLogger::log(ex);
                }
            };
        }
    };

    // HTTPS server wrapper: create an SSL-aware CMDHandler for each TLS connection.
    class RPC_HTTPS_Server final : public org::minima::system::network::rpc::HTTPSServer {
    public:
        explicit RPC_HTTPS_Server(int port) : HTTPSServer(port) {}
        std::function<void()> getSocketHandler(std::shared_ptr<SSLClient> zSocket) override {
            return [zSocket]() {
                try {
                    org::minima::system::network::rpc::CMDHandler handler(zSocket);
                    handler.run();
                } catch (const std::exception& ex) {
                    org::minima::utils::MinimaLogger::log(ex);
                }
            };
        }
    };

    static std::string timeMillisToString(long long millis) {
        using namespace std::chrono;
        system_clock::time_point tp = system_clock::time_point(milliseconds(millis));
        std::time_t tt = system_clock::to_time_t(tp);
        std::tm tmval{};
    #ifdef _WIN32
        localtime_s(&tmval, &tt);
    #else
        localtime_r(&tt, &tmval);
    #endif
        std::ostringstream oss;
        oss << std::put_time(&tmval, "%c");
        return oss.str();
    }
} // anonymous namespace

NetworkManager::NetworkManager() {
    // Not shutting down
    mShuttingDown = false;

    // Calculate the local host
    calculateHostIP();

    // Is the P2P Enabled
    if (org::minima::system::params::GeneralParams::P2P_ENABLED) {
        // Create the Manager
        mP2PManager = std::unique_ptr<org::minima::utils::messages::MessageProcessor>(new org::minima::system::network::p2p::P2PManager());
    } else {
        // Create a Dummy listener
        mP2PManager = std::make_unique<DummyProcessor>("P2P_DUMMY");
    }

    // The main NIO server manager
    mNIOManager = std::make_unique<org::minima::system::network::minima::NIOManager>(*this);

    // Is the P2P2 enabled
    if (org::minima::system::params::GeneralParams::P2P2_ENABLED) {
        mP2P2Manager = std::unique_ptr<org::minima::utils::messages::MessageProcessor>(new org::minima::system::network::p2p2::P2P2Manager());
    } else {
        // Create a Dummy listener
        mP2P2Manager = std::make_unique<DummyProcessor>("P2P2_DUMMY");
    }

    // Do we start the RPC server. Refuse to expose RPC without a password: an
    // empty/unset password would let any client authenticate.
    if (org::minima::system::params::GeneralParams::RPC_ENABLED) {
        using org::minima::system::params::GeneralParams;
        if (GeneralParams::RPC_PASSWORD.empty() || GeneralParams::RPC_PASSWORD == "none") {
            org::minima::utils::MinimaLogger::log(
                "RPC enabled without a password - refusing to start the RPC server");
        } else {
            startRPC();
        }
    }
}

NetworkManager::~NetworkManager() = default;
NetworkManager::NetworkManager(NetworkManager&&) noexcept = default;
NetworkManager& NetworkManager::operator=(NetworkManager&&) noexcept = default;

void NetworkManager::calculateHostIP() {
    using org::minima::system::params::GeneralParams;

    // Has it been specified at the command line..?
    if (GeneralParams::IS_HOST_SET) {
        return;
    }

    try {
        // Start easy
        GeneralParams::MINIMA_HOST = "127.0.0.1";

        bool found = false;

    #ifdef _WIN32
        ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        ULONG family = AF_UNSPEC;
        ULONG outBufLen = 16384;
        std::vector<unsigned char> buffer(outBufLen);
        PIP_ADAPTER_ADDRESSES addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

        ULONG ret = GetAdaptersAddresses(family, flags, nullptr, addrs, &outBufLen);
        if (ret == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(outBufLen);
            addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
            ret = GetAdaptersAddresses(family, flags, nullptr, addrs, &outBufLen);
        }

        if (ret == NO_ERROR) {
            for (PIP_ADAPTER_ADDRESSES aa = addrs; aa != nullptr; aa = aa->Next) {
                if (aa->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                    continue;
                }
                if (aa->OperStatus != IfOperStatusUp) {
                    continue;
                }

                for (PIP_ADAPTER_UNICAST_ADDRESS ua = aa->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
                    if (!ua->Address.lpSockaddr) continue;
                    if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;

                    char host[NI_MAXHOST] = {0};
                    DWORD hostlen = NI_MAXHOST;
                    int rc = WSAAddressToStringA(ua->Address.lpSockaddr,
                                                 (DWORD)ua->Address.iSockaddrLength,
                                                 nullptr, host, &hostlen);
                    if (rc == 0) {
                        std::string ip(host);
                        auto pos = ip.find('%'); if (pos != std::string::npos) ip = ip.substr(0, pos);
                        pos = ip.find(':'); if (pos != std::string::npos) {
                            // skip IPv6
                            continue;
                        }
                        // This breaks P2P if we keep 127.0.0.1 only - update to actual IPv4
                        GeneralParams::MINIMA_HOST = ip;
                        // Do not break; prefer the last seen IPv4 if no Wi-Fi heuristic available
                    }
                }
            }
        }
    #else
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0) {
            for (struct ifaddrs* ifa = ifaddr; ifa != nullptr && !found; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr) continue;

                unsigned int flags = ifa->ifa_flags;
                if ((flags & IFF_LOOPBACK) || !(flags & IFF_UP)) {
                    continue; // skip loopback and inactive
                }

                if (ifa->ifa_addr->sa_family == AF_INET) {
                    char host[INET_ADDRSTRLEN] = {0};
                    auto* sin = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                    if (inet_ntop(AF_INET, &(sin->sin_addr), host, sizeof(host))) {
                        std::string ip(host);
                        std::string name = ifa->ifa_name ? ifa->ifa_name : "";
                        // This breaks P2P if we stay on 127.0.0.1 only - update to actual IPv4
                        GeneralParams::MINIMA_HOST = ip;

                        // Prefer Wi-Fi interface names starting with "wl"
                        if (!name.empty() && name.rfind("wl", 0) == 0) {
                            found = true;
                            break;
                        }
                    }
                }
            }
            freeifaddrs(ifaddr);
        }
    #endif
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(std::string("ERROR calculating host IP : ") + e.what());
    }
}

org::minima::utils::json::JSONObject NetworkManager::getStatus() {
    return getStatus(false);
}

org::minima::utils::json::JSONObject NetworkManager::getStatus(bool zAll) {
    using org::minima::utils::json::JSONObject;
    using org::minima::system::params::GeneralParams;

    JSONObject stats;

    // Touch the UserDB as in Java
    auto& udb = org::minima::database::MinimaDB::getDB()->getUserDB();
    (void)udb;

    stats.put("host", GeneralParams::MINIMA_HOST);
    stats.put("hostset", GeneralParams::IS_HOST_SET);
    stats.put("port", GeneralParams::MINIMA_PORT);

    stats.put("connecting", mNIOManager ? mNIOManager->getNumberOfConnnectingClients() : 0);
    stats.put("connected",  mNIOManager ? mNIOManager->getNumberOfConnectedClients()   : 0);

    // RPC Stats
    JSONObject rpcjson;
    rpcjson.put("enabled", GeneralParams::RPC_ENABLED);
    rpcjson.put("port", GeneralParams::RPC_PORT);
    stats.put("rpc", rpcjson);

    if (!zAll) {
        return stats;
    }

    // P2P stats
    if (GeneralParams::P2P_ENABLED) {
        auto* p2p = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(mP2PManager.get());
        if (p2p) {
            stats.put("p2p", p2p->getStatus(false));
        } else {
            stats.put("p2p", std::string("disabled"));
        }
    } else {
        stats.put("p2p", std::string("disabled"));
    }

    // P2P2 stats
    if (GeneralParams::P2P2_ENABLED) {
        stats.put("p2p2", std::string("enabled"));
    } else {
        stats.put("p2p2", std::string("disabled"));
    }

    // Read / Write stats..
    if (mNIOManager) {
        org::minima::system::network::minima::NIOTraffic& traffic = mNIOManager->getTrafficListener();

        JSONObject readwrite;

        auto now_ms = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        long long start = traffic.getStartTime();
        long long diff = now_ms - start;
        long long mins = diff / (1000LL * 60LL);
        if (mins == 0) {
            mins = 1;
        }

        readwrite.put("from", timeMillisToString(start));
        readwrite.put("totalread",  org::minima::utils::MiniFormat::formatSize(traffic.getTotalRead()));
        readwrite.put("totalwrite", org::minima::utils::MiniFormat::formatSize(traffic.getTotalWrite()));
        readwrite.put("breakdown",  traffic.getBreakdown());

        long long speedread  = (traffic.getTotalRead()  / mins);
        long long speedwrite = (traffic.getTotalWrite() / mins);
        readwrite.put("read",  org::minima::utils::MiniFormat::formatSize(speedread) + "/min");
        readwrite.put("write", org::minima::utils::MiniFormat::formatSize(speedwrite) + "/min");

        stats.put("traffic", readwrite);
    }

    return stats;
}

void NetworkManager::startRPC() {
    using org::minima::system::params::GeneralParams;

    if (!GeneralParams::RPC_SSL) {
        if (!mRPCServer) {
            // HTTP server (inherits Server). Construct first, then start the
            // accept thread once the most-derived object is fully built.
            mRPCServer = std::make_unique<RPC_HTTP_Server>(GeneralParams::RPC_PORT);
            static_cast<org::minima::system::network::rpc::HTTPServer*>(mRPCServer.get())->start();
        }
    } else {
        if (!mRPCSServer) {
            // HTTPS server (does not inherit Server in this C++ mapping)
            mRPCSServer = std::make_unique<RPC_HTTPS_Server>(GeneralParams::RPC_PORT);
            mRPCSServer->start();
        }
    }
}

void NetworkManager::stopRPC() {
    // Stop the RPC
    if (mRPCServer) {
        mRPCServer->shutdown();
        mRPCServer.reset();
    }
    if (mRPCSServer) {
        mRPCSServer->shutdown();
        mRPCSServer.reset();
    }
}

void NetworkManager::shutdownNetwork() {
    using org::minima::system::params::GeneralParams;

    // We are trying to shutdown
    mShuttingDown = true;

    // And the RPC
    stopRPC();

    // Stop the NIO Manager
    if (mNIOManager) {
        mNIOManager->PostMessage(org::minima::system::network::minima::NIOManager::NIO_SHUTDOWN);
    }

    // Send a message to the P2P
    if (GeneralParams::P2P_ENABLED) {
        auto* mgr = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(mP2PManager.get());
        if (mgr) {
            mgr->shutdown();
        }
    } else if (mP2PManager) {
        mP2PManager->stopMessageProcessor();
    }

    // Send a message to the P2P2
    if (GeneralParams::P2P2_ENABLED) {
        auto* mgr2 = dynamic_cast<org::minima::system::network::p2p2::P2P2Manager*>(mP2P2Manager.get());
        if (mgr2) {
            mgr2->shutdown();
        }
    } else if (mP2P2Manager) {
        mP2P2Manager->stopMessageProcessor();
    }
}

bool NetworkManager::isShutDownComplete() {
    bool nio  = mNIOManager ? mNIOManager->isShutdownComplete() : true;
    bool p2p  = mP2PManager ? mP2PManager->isShutdownComplete() : true;
    bool p2p2 = mP2P2Manager ? mP2P2Manager->isShutdownComplete() : true;
    return nio && p2p && p2p2;
}

void NetworkManager::hardShutDown() {
    try {
        if (mNIOManager) {
            mNIOManager->hardShutDown();
        }
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
    }
    if (mP2PManager) {
        mP2PManager->stopMessageProcessor();
    }
    if (mP2P2Manager) {
        mP2P2Manager->stopMessageProcessor();
    }
}

org::minima::utils::messages::MessageProcessor& NetworkManager::getP2PManager() {
    return *mP2PManager;
}

org::minima::utils::messages::MessageProcessor& NetworkManager::getP2P2Manager() {
    return *mP2P2Manager;
}

org::minima::system::network::minima::NIOManager& NetworkManager::getNIOManager() {
    return *mNIOManager;
}

} // namespace network
} // namespace system
} // namespace minima
} // namespace org