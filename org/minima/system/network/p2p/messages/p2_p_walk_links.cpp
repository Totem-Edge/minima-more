#include "org/minima/system/network/p2p/messages/p2_p_walk_links.hpp"

#include <algorithm>
#include <utility>

#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

using org::minima::objects::base::MiniData;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

// Constructors
P2PWalkLinks::P2PWalkLinks() = default;

P2PWalkLinks::P2PWalkLinks(bool walkInLinks, bool isJoiningWalk, const std::string& targetUid)
    : m_targetUidForNextHop(targetUid),
      m_walkInLinks(walkInLinks),
      m_isJoiningWalk(isJoiningWalk),
      m_isClientWalk(false),
      m_availableNoneP2PConnectionSlots(0),
      m_isReturning(false),
      m_pathTaken(),
      m_secret(MiniData::getRandomData(8)) {}

P2PWalkLinks::P2PWalkLinks(bool walkInLinks, bool isReturning, const std::vector<InetSocketAddress>& pathTaken)
    : m_walkInLinks(walkInLinks),
      m_isJoiningWalk(false),
      m_isClientWalk(false),
      m_availableNoneP2PConnectionSlots(0),
      m_isReturning(isReturning),
      m_pathTaken(pathTaken),
      m_secret(MiniData::getRandomData(8)) {}

// Static factory
P2PWalkLinks P2PWalkLinks::readFromJSON(const JSONObject& json) {
    P2PWalkLinks msgWalkLinks;
    msgWalkLinks.fromJson(json);
    return msgWalkLinks;
}

// JSON serialization
JSONObject P2PWalkLinks::toJson() const {
    JSONObject contents;
    contents.put("secret", m_secret.toString());
    contents.put("walkInLinks", m_walkInLinks);
    contents.put("isJoiningWalk", m_isJoiningWalk);
    contents.put("isClientWalk", m_isClientWalk);
    contents.put("availableClientSlots", m_availableNoneP2PConnectionSlots);
    contents.put("isReturning", m_isReturning);
    contents.put("pathTaken", InetSocketAddressIO::addressesListToJSON(m_pathTaken));

    JSONObject main;
    main.put("walk_links", contents);
    return main;
}

void P2PWalkLinks::fromJson(const JSONObject& json) {
    // Extract "walk_links" subobject
    const auto& any_links = json.get("walk_links");
    const JSONObject& contents = std::any_cast<const JSONObject&>(any_links);

    if (contents.containsKey("secret")) {
        const auto& any_secret = contents.get("secret");
        const std::string& secStr = std::any_cast<const std::string&>(any_secret);
        this->setSecret(MiniData(secStr));
    }
    if (contents.containsKey("walkInLinks")) {
        const auto& any_val = contents.get("walkInLinks");
        bool val = std::any_cast<bool>(any_val);
        this->setWalkInLinks(val);
    }
    if (contents.containsKey("isJoiningWalk")) {
        const auto& any_val = contents.get("isJoiningWalk");
        bool val = std::any_cast<bool>(any_val);
        this->setJoiningWalk(val);
    }
    if (contents.containsKey("isClientWalk")) {
        const auto& any_val = contents.get("isClientWalk");
        bool val = std::any_cast<bool>(any_val);
        this->setClientWalk(val);
    }
    if (contents.containsKey("availableClientSlots")) {
        int slots = InetSocketAddressIO::safeReadInt(contents, "availableClientSlots");
        this->setAvailableNoneP2PConnectionSlots(slots);
    }
    if (contents.containsKey("isReturning")) {
        const auto& any_val = contents.get("isReturning");
        bool val = std::any_cast<bool>(any_val);
        this->setReturning(val);
    }
    if (contents.containsKey("pathTaken")) {
        const auto& any_arr = contents.get("pathTaken");
        const JSONArray& arr = std::any_cast<const JSONArray&>(any_arr);
        this->setPathTaken(InetSocketAddressIO::addressesJSONToList(arr));
    }
}

// Path management
void P2PWalkLinks::addHopToPath(const InetSocketAddress& address) {
    // In Java, null would throw; here ref cannot be null, so just add.
    m_pathTaken.push_back(address);
}

void P2PWalkLinks::addHopToPath(const InetSocketAddress* address) {
    if (address == nullptr) {
        throw std::invalid_argument("Address is null");
    }
    m_pathTaken.push_back(*address);
}

const InetSocketAddress* P2PWalkLinks::getPreviousNode(const InetSocketAddress& thisAddress) const {
    // Compare by host address and port to mimic Java InetSocketAddress.equals()
    auto equalsSock = [](const InetSocketAddress& a, const InetSocketAddress& b) -> bool {
        return a.getAddress().getHostAddress() == b.getAddress().getHostAddress() &&
               a.getPort() == b.getPort();
    };

    int index = -1;
    for (std::size_t i = 0; i < m_pathTaken.size(); ++i) {
        if (equalsSock(m_pathTaken[i], thisAddress)) {
            index = static_cast<int>(i);
            break;
        }
    }

    if (index > 0) {
        return &m_pathTaken[static_cast<std::size_t>(index - 1)];
    }
    return nullptr;
}

// Getters/Setters
const std::string& P2PWalkLinks::getTargetUidForNextHop() const {
    return m_targetUidForNextHop;
}

void P2PWalkLinks::setTargetUidForNextHop(const std::string& targetUidForNextHop) {
    m_targetUidForNextHop = targetUidForNextHop;
}

MiniData P2PWalkLinks::getSecret() const {
    return m_secret;
}

void P2PWalkLinks::setSecret(const MiniData& secret) {
    m_secret = secret;
}

bool P2PWalkLinks::isWalkInLinks() const {
    return m_walkInLinks;
}

void P2PWalkLinks::setWalkInLinks(bool walkInLinks) {
    m_walkInLinks = walkInLinks;
}

bool P2PWalkLinks::isJoiningWalk() const {
    return m_isJoiningWalk;
}

void P2PWalkLinks::setJoiningWalk(bool joiningWalk) {
    m_isJoiningWalk = joiningWalk;
}

bool P2PWalkLinks::isClientWalk() const {
    return m_isClientWalk;
}

void P2PWalkLinks::setClientWalk(bool clientWalk) {
    m_isClientWalk = clientWalk;
}

int P2PWalkLinks::getAvailableNoneP2PConnectionSlots() const {
    return m_availableNoneP2PConnectionSlots;
}

void P2PWalkLinks::setAvailableNoneP2PConnectionSlots(int availableNoneP2PConnectionSlots) {
    m_availableNoneP2PConnectionSlots = availableNoneP2PConnectionSlots;
}

bool P2PWalkLinks::isReturning() const {
    return m_isReturning;
}

void P2PWalkLinks::setReturning(bool returning) {
    m_isReturning = returning;
}

const std::vector<InetSocketAddress>& P2PWalkLinks::getPathTaken() const {
    return m_pathTaken;
}

void P2PWalkLinks::setPathTaken(const std::vector<InetSocketAddress>& pathTaken) {
    m_pathTaken = pathTaken;
}

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org