#pragma once

#include <string>
#include <vector>
#include <unordered_set>

namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOClientInfo;
} } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p2 {

class P2P2Functions {
public:
    enum class Level {
        NODE_RUNNER_MSG, INFO, DEBUG
    };

    // Default messages and connection events
    static const std::string P2P_INIT;
    static const std::string P2P_SHUTDOWN;
    static const std::string P2P_CONNECTED;
    static const std::string P2P_DISCONNECTED;
    static const std::string P2P_NOCONNECT;
    static const std::string P2P_MESSAGE;

    // Addresses for local interfaces (lazy populated)
    static const std::unordered_set<std::string>& getLocalAddresses();

    // Invalid peers list management
    static void addInvalidPeer(const std::string& zHostPost);
    static bool isInvalidPeer(const std::string& zHostPost);
    static void clearInvalidPeers();

    // Helpers
    static bool isIPv6(const std::string& fullhost);
    static bool isIPLocal(const std::string& fullhost);
    static bool isNetAvailable();

    // Networking calls
    static void connect(const std::string& zHost, int zPort);
    static bool checkConnect(const std::string& zHost, int zPort);
    static void disconnect(const std::string& zUID);

    // Connection info
    static std::vector<org::minima::system::network::minima::NIOClientInfo*> getAllConnections();
    static std::vector<org::minima::system::network::minima::NIOClientInfo*> getAllConnectedConnections();
    static org::minima::system::network::minima::NIOClientInfo* getNIOCLientInfo(const std::string& zUID);

    // Test main (translation of Java's main)
    static int main(int argc, char* argv[]);

private:
    static std::unordered_set<std::string> getAllNetworkInterfaceAddresses();

    // Static state
    static std::unordered_set<std::string> sInvalidPeers;
    static bool sLocalAddressesInit;
    static std::unordered_set<std::string> sLocalAddresses;
};

} // namespace p2p2
} // namespace network
} // namespace system
} // namespace minima
} // namespace org