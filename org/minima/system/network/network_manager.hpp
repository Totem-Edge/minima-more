#pragma once

#include <memory>
#include <string>

namespace org { namespace minima { namespace utils { namespace messages { class MessageProcessor; } } } }
namespace org { namespace minima { namespace system { namespace network { namespace minima { class NIOManager; class NIOTraffic; } } } } }
namespace org { namespace minima { namespace system { namespace network { namespace rpc { class Server; class HTTPSServer; } } } } }
namespace org { namespace minima { namespace system { namespace network { namespace p2p { class P2PManager; } } } } }
namespace org { namespace minima { namespace system { namespace network { namespace p2p2 { class P2P2Manager; } } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace network {

class NetworkManager {
public:
    // Public to mirror Java field visibility
    bool mShuttingDown = false;

    NetworkManager();

    // PIMPL-FIX for unique_ptr to forward-declared types
    virtual ~NetworkManager();
    NetworkManager(NetworkManager&&) noexcept;
    NetworkManager& operator=(NetworkManager&&) noexcept;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    void calculateHostIP();

    // Status
    org::minima::utils::json::JSONObject getStatus();
    org::minima::utils::json::JSONObject getStatus(bool zAll);

    // RPC control
    void startRPC();
    void stopRPC();

    // Shutdowns
    void shutdownNetwork();
    bool isShutDownComplete();
    void hardShutDown();

    // Accessors
    org::minima::utils::messages::MessageProcessor& getP2PManager();
    org::minima::utils::messages::MessageProcessor& getP2P2Manager();
    org::minima::system::network::minima::NIOManager& getNIOManager();

private:
    std::unique_ptr<org::minima::system::network::minima::NIOManager> mNIOManager;
    std::unique_ptr<org::minima::utils::messages::MessageProcessor>   mP2PManager;
    std::unique_ptr<org::minima::utils::messages::MessageProcessor>   mP2P2Manager;

    // Two pointers due to differing base classes in C++ mapping
    std::unique_ptr<org::minima::system::network::rpc::Server>        mRPCServer;   // HTTP (inherits Server)
    std::unique_ptr<org::minima::system::network::rpc::HTTPSServer>   mRPCSServer;  // HTTPS (does NOT inherit Server)
};

} // namespace network
} // namespace system
} // namespace minima
} // namespace org