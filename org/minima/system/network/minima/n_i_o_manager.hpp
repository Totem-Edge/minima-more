#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdint>

#include "org/minima/utils/messages/message_processor.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Forward declarations (namespaced per Pitfall 10)
namespace org { namespace minima { namespace system { namespace network { class NetworkManager; } } } }
namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOServer;
class NIOClient;
class NIOClientInfo;
class NIOTraffic;
class NIOMessage;
} } } } }
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace database { namespace archive { class ArchiveManager; } } } }
namespace org { namespace minima { namespace objects { class Greeting; class TxBlock; class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniByte; class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class Message; class TimerMessage; } } } }
// Streamable forward declaration (Pitfall 10)
namespace org { namespace minima { namespace utils { class Streamable; } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

class NIOManager : public org::minima::utils::messages::MessageProcessor {
public:
    // Message constants
    inline static constexpr const char* NIO_SERVERSTARTED   = "NIO_SERVERSTARTED";
    inline static constexpr const char* NIO_SHUTDOWN        = "NIO_SHUTDOWN";
    inline static constexpr const char* NIO_CONNECT         = "NIO_CONNECT";
    inline static constexpr const char* NIO_CONNECTATTEMPT  = "NIO_CONNECTATTEMPT";
    inline static constexpr const char* NIO_NEWCONNECTION   = "NIO_NEWCONNECT";
    inline static constexpr const char* NIO_DISCONNECT      = "NIO_DISCONNECT";
    inline static constexpr const char* NIO_DISCONNECTED    = "NIO_DISCONNECTED";
    inline static constexpr const char* NIO_DISCONNECTALL   = "NIO_DISCONNECTALL";
    inline static constexpr const char* NIO_RECONNECT       = "NIO_RECONNECT";
    inline static constexpr const char* NIO_INCOMINGMSG     = "NIO_NEWMSG";
    inline static constexpr const char* NIO_TXPOWREQ        = "NIO_REQTXPOW";
    inline static constexpr const char* NIO_SYNCTXBLOCK     = "NIO_SYNCTXBLOCK";
    inline static constexpr const char* NIO_CHECKLASTMSG    = "NIO_CHECKLASTMSG";
    inline static constexpr const char* NIO_HEALTHCHECK     = "NIO_HEALTHCHECK";

    // Limits
    static long long MAX_ARCHIVE_WRITE; // 50 MB default

    // Constructor/Destructor (PIMPL fix due to unique_ptr to incomplete types)
    explicit NIOManager(org::minima::system::network::NetworkManager& zNetManager);
    virtual ~NIOManager();
    NIOManager(NIOManager&&) noexcept = delete;
    NIOManager& operator=(NIOManager&&) noexcept = delete;

    // Delete copy
    NIOManager(const NIOManager&) = delete;
    NIOManager& operator=(const NIOManager&) = delete;

    // Accessors
    org::minima::system::network::minima::NIOServer* getNIOServer();
    int getNumberOfConnectedClients();
    int getNumberOfConnnectingClients();

    // Connections info
    std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>> getAllConnectionInfo();
    std::vector<org::minima::system::network::minima::NIOClient*> getAllValidConnectedClients();
    std::vector<org::minima::system::network::minima::NIOClient*> getAllValidOutGoingConnectedClients();
    org::minima::utils::json::JSONObject getAllConnectedDetails();

    org::minima::system::network::minima::NIOClient* checkConnected(const std::string& zHost, bool zOnlyConnected);
    org::minima::system::network::minima::NIOClient* getNIOClient(const std::string& zHost);
    org::minima::system::network::minima::NIOClient* getNIOClientFromUID(const std::string& zUID);

    void hardShutDown();

    org::minima::system::network::minima::NIOTraffic& getTrafficListener();

    // Disconnect helpers
    void disconnect(const std::string& zClientUID);
    void disconnect(const std::string& zClientUID, bool zRemoveP2P);

    // Static send helpers
    static void sendNetworkMessageAll(const org::minima::objects::base::MiniByte& zType,
                                      org::minima::utils::Streamable& zObject);
    static void sendNetworkMessage(const std::string& zUID,
                                   const org::minima::objects::base::MiniByte& zType,
                                   const org::minima::utils::Streamable& zObject);

    static org::minima::objects::base::MiniData createNIOMessage(const org::minima::objects::base::MiniByte& zType,
                                                                 const org::minima::utils::Streamable& zObject);

    // PING helper
    static std::shared_ptr<org::minima::objects::Greeting> sendPingMessage(const std::string& zHost, int zPort, bool suppressErrorMessage);

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    void connectAttempt(const std::shared_ptr<org::minima::system::network::minima::NIOClient>& zNIOClient);

private:
    // Timers and params
    int RECONNECT_ATTEMPTS = 3;
    long long LASTREAD_CHECKER = static_cast<long long>(1000) * 120;
    long long MAX_LASTREAD_CHECKER = static_cast<long long>(1000) * 60 * 10;
    long long SYNC_MAX_TIME; // set from GeneralParams.NUMBER_DAYS_ARCHIVE at construction
    static constexpr long RECONNECT_TIMER = 30000;

    // Main Network Manager (non-owning)
    org::minima::system::network::NetworkManager* mNetworkManager;

    // The MAIN Minima Server
    std::unique_ptr<org::minima::system::network::minima::NIOServer> mNIOServer;

    // Clients we are trying to connect to
    std::unordered_map<std::string, std::shared_ptr<org::minima::system::network::minima::NIOClient>> mConnectingClients;
    std::mutex mConnectMutex;

    // Sync helpers (not heavily used)
    std::mutex mSyncMutex;
    std::unordered_set<std::string> mAwaitingConnect;

    // Traffic monitor
    std::unique_ptr<org::minima::system::network::minima::NIOTraffic> mTrafficListener;

    // Thread pool (ExecutorService equivalent) - defined in .cpp
    class ThreadPool;
    std::unique_ptr<ThreadPool> mThreadPool;
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org