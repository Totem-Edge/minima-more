#include "org/minima/system/network/p2p/p2_p_state.hpp"

#include <utility>

#include "org/minima/system/network/p2p/params/p2_p_params.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

bool P2PState::InetSocketAddressLess::operator()(const InetSocketAddress& a, const InetSocketAddress& b) const {
    // Compare by host (prefer original host), then by port
    const std::string& ah = a.originalHost().empty() ? a.getAddress().getHostAddress() : a.originalHost();
    const std::string& bh = b.originalHost().empty() ? b.getAddress().getHostAddress() : b.originalHost();
    if (ah < bh) return true;
    if (ah > bh) return false;
    return a.getPort() < b.getPort();
}

P2PState::P2PState()
    : m_inLinks()
    , m_outLinks()
    , m_notAcceptingConnP2PLinks()
    , m_noneP2PLinks()
    , m_allLinks()
    , m_knownPeers()
    , m_myMinimaAddress("", 9001) // Java: new InetSocketAddress(9001); host unspecified
    , m_ipReqSecret()
    , m_isAcceptingInLinks(true)
    , m_maxNumNoneP2PConnections(org::minima::system::network::p2p::params::P2PParams::TGT_NUM_NONE_P2P_LINKS)
    , m_maxNumP2PConnections(org::minima::system::network::p2p::params::P2PParams::TGT_NUM_LINKS)
    , m_doingDiscoveryConnection(true)
    , m_startupComplete(false)
    , m_loopDelay(org::minima::system::network::p2p::params::P2PParams::LOOP_DELAY)
    , m_noConnect(false)
    , m_isHostSet(false) {
    // Creates a new empty state
}

std::set<P2PState::InetSocketAddress, P2PState::InetSocketAddressLess>& P2PState::getKnownPeers() {
    return m_knownPeers;
}

std::vector<P2PState::InetSocketAddress> P2PState::getKnownPeersCopy() const {
    std::vector<InetSocketAddress> copy;
    copy.reserve(m_knownPeers.size());
    for (const auto& addr : m_knownPeers) {
        copy.push_back(addr);
    }
    return copy;
}

bool P2PState::isAcceptingInLinks() const {
    return m_isAcceptingInLinks;
}

void P2PState::setAcceptingInLinks(bool acceptingInLinks) {
    m_isAcceptingInLinks = acceptingInLinks;
}

std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getInLinks() {
    return m_inLinks;
}

const std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getInLinks() const {
    return m_inLinks;
}

void P2PState::setInLinks(const std::unordered_map<std::string, InetSocketAddress>& inLinks) {
    m_inLinks = inLinks;
}

std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getOutLinks() {
    return m_outLinks;
}

const std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getOutLinks() const {
    return m_outLinks;
}

void P2PState::setOutLinks(const std::unordered_map<std::string, InetSocketAddress>& outLinks) {
    m_outLinks = outLinks;
}

std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getNoneP2PLinks() {
    return m_noneP2PLinks;
}

const std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getNoneP2PLinks() const {
    return m_noneP2PLinks;
}

void P2PState::setNoneP2PLinks(const std::unordered_map<std::string, InetSocketAddress>& noneP2PLinks) {
    m_noneP2PLinks = noneP2PLinks;
}

int P2PState::getMaxNumNoneP2PConnections() const {
    return m_maxNumNoneP2PConnections;
}

void P2PState::setMaxNumNoneP2PConnections(int maxNumNoneP2PConnections) {
    m_maxNumNoneP2PConnections = maxNumNoneP2PConnections;
}

bool P2PState::isDoingDiscoveryConnection() const {
    return m_doingDiscoveryConnection;
}

void P2PState::setDoingDiscoveryConnection(bool doingDiscoveryConnection) {
    m_doingDiscoveryConnection = doingDiscoveryConnection;
}

const P2PState::InetSocketAddress& P2PState::getMyMinimaAddress() const {
    return m_myMinimaAddress;
}

void P2PState::setMyMinimaAddress(const std::string& host) {
    m_myMinimaAddress = InetSocketAddress(host, org::minima::system::params::GeneralParams::MINIMA_PORT);
}

void P2PState::setMyMinimaAddress(const InetSocketAddress& myMinimaAddress) {
    m_myMinimaAddress = myMinimaAddress;
}

const org::minima::objects::base::MiniData& P2PState::getIpReqSecret() const {
    return m_ipReqSecret;
}

void P2PState::setIpReqSecret(const org::minima::objects::base::MiniData& ipReqSecret) {
    m_ipReqSecret = ipReqSecret;
}

std::int64_t P2PState::getLoopDelay() const {
    return m_loopDelay;
}

void P2PState::setLoopDelay(std::int64_t loopDelay) {
    m_loopDelay = loopDelay;
}

void P2PState::setLoopDelayToParamValue() {
    m_loopDelay = org::minima::system::network::p2p::params::P2PParams::LOOP_DELAY;
}

std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getNotAcceptingConnP2PLinks() {
    return m_notAcceptingConnP2PLinks;
}

const std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getNotAcceptingConnP2PLinks() const {
    return m_notAcceptingConnP2PLinks;
}

void P2PState::setNotAcceptingConnP2PLinks(const std::unordered_map<std::string, InetSocketAddress>& notAcceptingConnP2PLinks) {
    m_notAcceptingConnP2PLinks = notAcceptingConnP2PLinks;
}

org::minima::utils::json::JSONObject P2PState::toJson() const {
    using org::minima::utils::json::JSONObject;
    using org::minima::system::network::p2p::messages::InetSocketAddressIO;

    JSONObject json;

    // Address string - mimic Java's toString().replace("/", "") by using "host:port" without slashes
    json.put("address", addressToString(m_myMinimaAddress));

    json.put("is_mobile", org::minima::system::params::GeneralParams::IS_MOBILE);
    json.put("is_accepting_connections", m_isAcceptingInLinks);

    auto inVals   = valuesToVector(m_inLinks);
    auto outVals  = valuesToVector(m_outLinks);
    auto naVals   = valuesToVector(m_notAcceptingConnP2PLinks);
    auto noneVals = valuesToVector(m_noneP2PLinks);

    json.put("InLinks", InetSocketAddressIO::addressesListToJSONArray(inVals));
    json.put("OutLinks", InetSocketAddressIO::addressesListToJSONArray(outVals));
    json.put("NotAcceptingConnP2PLinks", InetSocketAddressIO::addressesListToJSONArray(naVals));
    json.put("NoneP2PLinks", InetSocketAddressIO::addressesListToJSONArray(noneVals));
    json.put("numAllLinks", static_cast<std::int64_t>(m_allLinks.size()));
    json.put("numKnownPeers", static_cast<std::int64_t>(m_knownPeers.size()));

    return json;
}

bool P2PState::isNoConnect() const {
    return m_noConnect;
}

void P2PState::setNoConnect(bool noConnect) {
    m_noConnect = noConnect;
}

bool P2PState::isHostSet() const {
    return m_isHostSet;
}

void P2PState::setHostSet(bool hostSet) {
    m_isHostSet = hostSet;
}

int P2PState::getMaxNumP2PConnections() const {
    return m_maxNumP2PConnections;
}

void P2PState::setMaxNumP2PConnections(int maxNumP2PConnections) {
    m_maxNumP2PConnections = maxNumP2PConnections;
}

std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getAllLinks() {
    return m_allLinks;
}

const std::unordered_map<std::string, P2PState::InetSocketAddress>& P2PState::getAllLinks() const {
    return m_allLinks;
}

void P2PState::setAllLinks(const std::unordered_map<std::string, InetSocketAddress>& allLinks) {
    m_allLinks = allLinks;
}

bool P2PState::isStartupComplete() const {
    return m_startupComplete;
}

void P2PState::setStartupComplete(bool startupComplete) {
    m_startupComplete = startupComplete;
}

std::string P2PState::addressToString(const InetSocketAddress& addr) {
    const std::string& host = !addr.originalHost().empty()
                                ? addr.originalHost()
                                : addr.getAddress().getHostAddress();
    // If host empty, mimic Java "/:port" then replace "/" -> "" gives ":port"
    if (host.empty()) {
        return std::string(":") + std::to_string(addr.getPort());
    }
    return host + ":" + std::to_string(addr.getPort());
}

std::vector<P2PState::InetSocketAddress> P2PState::valuesToVector(
    const std::unordered_map<std::string, InetSocketAddress>& m) {
    std::vector<InetSocketAddress> v;
    v.reserve(m.size());
    for (const auto& kv : m) {
        v.push_back(kv.second);
    }
    return v;
}

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org