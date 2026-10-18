#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"

// Forward declarations to minimize header dependencies
namespace org { namespace minima { namespace utils { namespace json {
class JSONObject;
class JSONArray;
}}}}

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
class P2PState;
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

class P2PGreeting {
public:
    // Types from imported header
    using InetSocketAddress = org::minima::system::network::p2p::messages::InetSocketAddress;

    // Constructors
    P2PGreeting();
    explicit P2PGreeting(const org::minima::system::network::p2p::P2PState& state);
    P2PGreeting(int myMinimaPort, const std::vector<InetSocketAddress>& knownPeers);
    P2PGreeting(int myMinimaPort,
                bool isAcceptingInLinks,
                const std::vector<InetSocketAddress>& outLinks,
                const std::vector<InetSocketAddress>& inLinks,
                int numNoneP2PConnections,
                int maxNumNoneP2PConnections,
                const std::vector<InetSocketAddress>& knownPeers);

    // JSON
    static P2PGreeting fromJSON(const org::minima::utils::json::JSONObject& jsonObject);
    org::minima::utils::json::JSONObject toJson() const;

    // Getters / Setters
    int getMyMinimaPort() const;
    void setMyMinimaPort(int myMinimaPort);

    bool isAcceptingInLinks() const;
    void setAcceptingInLinks(bool acceptingInLinks);

    const std::vector<InetSocketAddress>& getOutLinks() const;
    void setOutLinks(const std::vector<InetSocketAddress>& outLinks);

    const std::vector<InetSocketAddress>& getInLinks() const;
    void setInLinks(const std::vector<InetSocketAddress>& inLinks);

    int getNumNoneP2PConnections() const;
    void setNumNoneP2PConnections(int numNoneP2PConnections);

    int getMaxNumNoneP2PConnections() const;
    void setMaxNumNoneP2PConnections(int maxNumNoneP2PConnections);

    const std::vector<InetSocketAddress>& getKnownPeers() const;
    void setKnownPeers(const std::vector<InetSocketAddress>& knownPeers);

    // Equality (replicates Java equals logic)
    bool equals(const P2PGreeting& other) const;
    bool operator==(const P2PGreeting& other) const { return equals(other); }
    bool operator!=(const P2PGreeting& other) const { return !equals(other); }

private:
    int m_myMinimaPort{0};
    bool m_isAcceptingInLinks{false};
    std::vector<InetSocketAddress> m_outLinks;
    std::vector<InetSocketAddress> m_inLinks;
    int m_numNoneP2PConnections{0};
    int m_maxNumNoneP2PConnections{0};
    std::vector<InetSocketAddress> m_knownPeers;
};

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org