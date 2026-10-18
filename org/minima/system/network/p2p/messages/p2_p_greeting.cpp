#include "org/minima/system/network/p2p/messages/p2_p_greeting.hpp"

#include <algorithm>
#include <random>

#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/network/p2p/p2_p_state.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

using org::minima::system::params::GeneralParams;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

// Helper: extract JSONArray from JSONObject with default empty
namespace {
static JSONArray getJSONArrayOrDefault(const JSONObject& obj, const std::string& key) {
    if (!obj.containsKey(key)) {
        return JSONArray();
    }

    const std::any& v = obj.get(key);

    if (auto parr = std::any_cast<JSONArray>(&v)) {
        return *parr;
    }
    if (auto parr_const = std::any_cast<const JSONArray>(&v)) {
        return *parr_const;
    }
    // Try pointer forms if stored as pointer in std::any
    auto& nv = const_cast<std::any&>(v);
    if (auto parr_ptr = std::any_cast<JSONArray*>(&nv)) {
        if (parr_ptr && *parr_ptr) {
            return **parr_ptr;
        }
    }
    if (auto parr_cptr = std::any_cast<const JSONArray*>(&nv)) {
        if (parr_cptr && *parr_cptr) {
            return **parr_cptr;
        }
    }
    return JSONArray();
}
} // anonymous namespace

P2PGreeting::P2PGreeting() = default;

P2PGreeting::P2PGreeting(const org::minima::system::network::p2p::P2PState& state) {
    // Mirror Java constructor behavior
    m_myMinimaPort = GeneralParams::MINIMA_PORT;
    m_isAcceptingInLinks = state.isAcceptingInLinks();

    // Copy values of OutLinks map into vector
    m_outLinks.clear();
    for (const auto& kv : state.getOutLinks()) {
        m_outLinks.push_back(kv.second);
    }

    // Copy values of InLinks map into vector
    m_inLinks.clear();
    for (const auto& kv : state.getInLinks()) {
        m_inLinks.push_back(kv.second);
    }

    m_numNoneP2PConnections = static_cast<int>(state.getNoneP2PLinks().size());
    m_maxNumNoneP2PConnections = state.getMaxNumNoneP2PConnections();

    // Use const-correct copy of known peers (returns vector)
    m_knownPeers = state.getKnownPeersCopy();

    // Append own address if present and accepting
    if (m_isAcceptingInLinks) {
        const InetSocketAddress* myaddr = &state.getMyMinimaAddress();
        if (myaddr != nullptr) {
            m_knownPeers.push_back(*myaddr);
        }
    }

    // Shuffle the known peers
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(m_knownPeers.begin(), m_knownPeers.end(), gen);
}

P2PGreeting::P2PGreeting(int myMinimaPort, const std::vector<InetSocketAddress>& knownPeers)
    : m_myMinimaPort(myMinimaPort),
      m_isAcceptingInLinks(false),
      m_knownPeers(knownPeers) {}

P2PGreeting::P2PGreeting(int myMinimaPort,
                         bool isAcceptingInLinks,
                         const std::vector<InetSocketAddress>& outLinks,
                         const std::vector<InetSocketAddress>& inLinks,
                         int numNoneP2PConnections,
                         int maxNumNoneP2PConnections,
                         const std::vector<InetSocketAddress>& knownPeers)
    : m_myMinimaPort(myMinimaPort),
      m_isAcceptingInLinks(isAcceptingInLinks),
      m_outLinks(outLinks),
      m_inLinks(inLinks),
      m_numNoneP2PConnections(numNoneP2PConnections),
      m_maxNumNoneP2PConnections(maxNumNoneP2PConnections),
      m_knownPeers(knownPeers) {}

P2PGreeting P2PGreeting::fromJSON(const JSONObject& jsonObject) {
    P2PGreeting greeting;

    greeting.setMyMinimaPort(InetSocketAddressIO::safeReadInt(jsonObject, "myMinimaPort"));

    bool accepting = false;
    try {
        if (jsonObject.containsKey("isAcceptingInLinks")) {
            accepting = jsonObject.getBoolean("isAcceptingInLinks");
        }
    } catch (...) {
        // default remains false
    }
    greeting.setAcceptingInLinks(accepting);

    // knownPeers (always read)
    {
        JSONArray arr = getJSONArrayOrDefault(jsonObject, "knownPeers");
        greeting.setKnownPeers(InetSocketAddressIO::addressesJSONToList(arr));
    }

    if (greeting.isAcceptingInLinks()) {
        // outLinks
        {
            JSONArray arr = getJSONArrayOrDefault(jsonObject, "outLinks");
            greeting.setOutLinks(InetSocketAddressIO::addressesJSONToList(arr));
        }
        // inLinks
        {
            JSONArray arr = getJSONArrayOrDefault(jsonObject, "inLinks");
            greeting.setInLinks(InetSocketAddressIO::addressesJSONToList(arr));
        }

        greeting.setNumNoneP2PConnections(InetSocketAddressIO::safeReadInt(jsonObject, "numNoneP2PConnections"));
        greeting.setMaxNumNoneP2PConnections(InetSocketAddressIO::safeReadInt(jsonObject, "maxNumNoneP2PConnections"));
    }

    return greeting;
}

JSONObject P2PGreeting::toJson() const {
    JSONObject json;
    json.put("myMinimaPort", m_myMinimaPort);
    json.put("isAcceptingInLinks", m_isAcceptingInLinks);

    if (m_isAcceptingInLinks) {
        json.put("numNoneP2PConnections", m_numNoneP2PConnections);
        json.put("maxNumNoneP2PConnections", m_maxNumNoneP2PConnections);
        json.put("outLinks", InetSocketAddressIO::addressesListToJSON(m_outLinks));
        json.put("inLinks", InetSocketAddressIO::addressesListToJSON(m_inLinks));
    }

    json.put("knownPeers", InetSocketAddressIO::addressesListToJSON(m_knownPeers));

    JSONObject message;
    message.put("greeting", json);
    return message;
}

int P2PGreeting::getMyMinimaPort() const {
    return m_myMinimaPort;
}

void P2PGreeting::setMyMinimaPort(int myMinimaPort) {
    m_myMinimaPort = myMinimaPort;
}

bool P2PGreeting::isAcceptingInLinks() const {
    return m_isAcceptingInLinks;
}

void P2PGreeting::setAcceptingInLinks(bool acceptingInLinks) {
    m_isAcceptingInLinks = acceptingInLinks;
}

const std::vector<P2PGreeting::InetSocketAddress>& P2PGreeting::getOutLinks() const {
    return m_outLinks;
}

void P2PGreeting::setOutLinks(const std::vector<InetSocketAddress>& outLinks) {
    m_outLinks = outLinks;
}

const std::vector<P2PGreeting::InetSocketAddress>& P2PGreeting::getInLinks() const {
    return m_inLinks;
}

void P2PGreeting::setInLinks(const std::vector<InetSocketAddress>& inLinks) {
    m_inLinks = inLinks;
}

int P2PGreeting::getNumNoneP2PConnections() const {
    return m_numNoneP2PConnections;
}

void P2PGreeting::setNumNoneP2PConnections(int numNoneP2PConnections) {
    m_numNoneP2PConnections = numNoneP2PConnections;
}

int P2PGreeting::getMaxNumNoneP2PConnections() const {
    return m_maxNumNoneP2PConnections;
}

void P2PGreeting::setMaxNumNoneP2PConnections(int maxNumNoneP2PConnections) {
    m_maxNumNoneP2PConnections = maxNumNoneP2PConnections;
}

const std::vector<P2PGreeting::InetSocketAddress>& P2PGreeting::getKnownPeers() const {
    return m_knownPeers;
}

void P2PGreeting::setKnownPeers(const std::vector<InetSocketAddress>& knownPeers) {
    m_knownPeers = knownPeers;
}

bool P2PGreeting::equals(const P2PGreeting& other) const {
    if (this == &other) {
        return true;
    }
    return m_myMinimaPort == other.getMyMinimaPort()
        && m_isAcceptingInLinks == other.isAcceptingInLinks()
        && m_knownPeers.size() == other.getKnownPeers().size();
}

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org