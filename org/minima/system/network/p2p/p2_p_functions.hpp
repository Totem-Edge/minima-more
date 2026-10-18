#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>

namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace system { namespace network { namespace minima { class NIOClientInfo; } } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

class P2PFunctions {
public:
    enum class Level {
        NODE_RUNNER_MSG,
        INFO,
        DEBUG
    };

    // Default messages
    inline static constexpr const char* P2P_INIT        = "P2P_INIT";
    inline static constexpr const char* P2P_SHUTDOWN    = "P2P_SHUTDOWN";
    inline static constexpr const char* P2P_CONNECTED   = "P2P_CONNECTED";
    inline static constexpr const char* P2P_DISCONNECTED= "P2P_DISCONNECTED";
    inline static constexpr const char* P2P_NOCONNECT   = "P2P_NOCONNECT";
    inline static constexpr const char* P2P_MESSAGE     = "P2P_MESSAGE";

    // Local addresses cache
    static const std::unordered_set<std::string>& getLocalAddresses();

    // Invalid peers management
    static void addInvalidPeer(const std::string& zHostPort);
    static bool isInvalidPeer(const std::string& zHostPort);
    static void clearInvalidPeers();

    // IP utilities
    static bool isIPv6(const std::string& fullhost);
    static bool isIPLocal(const std::string& fullhost);

    // Network check
    static bool isNetAvailable();

    // Connectivity
    static void connect(const std::string& zHost, int zPort);
    static bool checkConnect(const std::string& zHost, int zPort);
    static void disconnect(const std::string& zUID);

    // Connections listing
    static std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>> getAllConnections();
    static std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>> getAllConnectedConnections();

    // Lookup by UID
    static std::unique_ptr<org::minima::system::network::minima::NIOClientInfo> getNIOCLientInfo(const std::string& zUID);

    // Send P2P messages
    static void sendP2PMessage(const std::string& zUID, const org::minima::utils::json::JSONObject& zMessage);
    static void sendP2PMessageAll(const org::minima::utils::json::JSONObject& zMessage);

    // Logging
    static void log_node_runner(const std::string& message);
    static void log_info(const std::string& message);
    static void log_debug(const std::string& message);

    // Translated Java test main (not program entry point)
    static void main(const std::vector<std::string>& zArgs);

private:
    static std::unordered_set<std::string> getAllNetworkInterfaceAddresses();

private:
    static std::unique_ptr<std::unordered_set<std::string>> s_localAddresses;
    static std::unordered_set<std::string> s_invalidPeers;
};

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org