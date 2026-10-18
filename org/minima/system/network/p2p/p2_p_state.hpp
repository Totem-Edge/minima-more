#pragma once

#include <string>
#include <unordered_map>
#include <set>
#include <vector>
#include <cstdint>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

class P2PState {
public:
    using InetSocketAddress = org::minima::system::network::p2p::messages::InetSocketAddress;

    // Comparator for InetSocketAddress in std::set
    struct InetSocketAddressLess {
        bool operator()(const InetSocketAddress& a, const InetSocketAddress& b) const;
    };

    P2PState();

    // Known peers
    std::set<InetSocketAddress, InetSocketAddressLess>& getKnownPeers();
    std::vector<InetSocketAddress> getKnownPeersCopy() const;

    // InLinks / OutLinks / NoneP2P / NotAccepting / AllLinks
    std::unordered_map<std::string, InetSocketAddress>& getInLinks();
    const std::unordered_map<std::string, InetSocketAddress>& getInLinks() const;
    void setInLinks(const std::unordered_map<std::string, InetSocketAddress>& inLinks);

    std::unordered_map<std::string, InetSocketAddress>& getOutLinks();
    const std::unordered_map<std::string, InetSocketAddress>& getOutLinks() const;
    void setOutLinks(const std::unordered_map<std::string, InetSocketAddress>& outLinks);

    std::unordered_map<std::string, InetSocketAddress>& getNoneP2PLinks();
    const std::unordered_map<std::string, InetSocketAddress>& getNoneP2PLinks() const;
    void setNoneP2PLinks(const std::unordered_map<std::string, InetSocketAddress>& noneP2PLinks);

    std::unordered_map<std::string, InetSocketAddress>& getNotAcceptingConnP2PLinks();
    const std::unordered_map<std::string, InetSocketAddress>& getNotAcceptingConnP2PLinks() const;
    void setNotAcceptingConnP2PLinks(const std::unordered_map<std::string, InetSocketAddress>& notAcceptingConnP2PLinks);

    std::unordered_map<std::string, InetSocketAddress>& getAllLinks();
    const std::unordered_map<std::string, InetSocketAddress>& getAllLinks() const;
    void setAllLinks(const std::unordered_map<std::string, InetSocketAddress>& allLinks);

    // Accepting InLinks
    bool isAcceptingInLinks() const;
    void setAcceptingInLinks(bool acceptingInLinks);

    // Max connections
    int getMaxNumNoneP2PConnections() const;
    void setMaxNumNoneP2PConnections(int maxNumNoneP2PConnections);

    int getMaxNumP2PConnections() const;
    void setMaxNumP2PConnections(int maxNumP2PConnections);

    // Discovery connection
    bool isDoingDiscoveryConnection() const;
    void setDoingDiscoveryConnection(bool doingDiscoveryConnection);

    // My Minima Address
    const InetSocketAddress& getMyMinimaAddress() const;
    void setMyMinimaAddress(const std::string& host);
    void setMyMinimaAddress(const InetSocketAddress& myMinimaAddress);

    // IP request secret
    const org::minima::objects::base::MiniData& getIpReqSecret() const;
    void setIpReqSecret(const org::minima::objects::base::MiniData& ipReqSecret);

    // Loop delay
    std::int64_t getLoopDelay() const;
    void setLoopDelay(std::int64_t loopDelay);
    void setLoopDelayToParamValue();

    // Startup complete
    bool isStartupComplete() const;
    void setStartupComplete(bool startupComplete);

    // No Connect
    bool isNoConnect() const;
    void setNoConnect(bool noConnect);

    // Host set
    bool isHostSet() const;
    void setHostSet(bool hostSet);

    // JSON representation
    org::minima::utils::json::JSONObject toJson() const;

private:
    // Maps of connection types
    std::unordered_map<std::string, InetSocketAddress> m_inLinks;
    std::unordered_map<std::string, InetSocketAddress> m_outLinks;
    std::unordered_map<std::string, InetSocketAddress> m_notAcceptingConnP2PLinks;
    std::unordered_map<std::string, InetSocketAddress> m_noneP2PLinks;
    std::unordered_map<std::string, InetSocketAddress> m_allLinks;

    // Known peers
    std::set<InetSocketAddress, InetSocketAddressLess> m_knownPeers;

    // Host Minima Address
    InetSocketAddress m_myMinimaAddress;

    // Secret for IP request messages
    org::minima::objects::base::MiniData m_ipReqSecret;

    // Flags and parameters
    bool m_isAcceptingInLinks = true;
    int m_maxNumNoneP2PConnections = 0;
    int m_maxNumP2PConnections = 0;
    bool m_doingDiscoveryConnection = true;
    bool m_startupComplete = false;
    std::int64_t m_loopDelay = 0;
    bool m_noConnect = false;
    bool m_isHostSet = false;

    // Helpers
    static std::string addressToString(const InetSocketAddress& addr);
    static std::vector<InetSocketAddress> valuesToVector(const std::unordered_map<std::string, InetSocketAddress>& m);
};

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org