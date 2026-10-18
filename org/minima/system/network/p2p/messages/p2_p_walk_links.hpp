#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

class P2PWalkLinks {
public:
    // Constructors
    P2PWalkLinks();
    P2PWalkLinks(bool walkInLinks, bool isJoiningWalk, const std::string& targetUid);
    P2PWalkLinks(bool walkInLinks, bool isReturning, const std::vector<InetSocketAddress>& pathTaken);

    // Static factory (Java: readFromJSON)
    static P2PWalkLinks readFromJSON(const org::minima::utils::json::JSONObject& json);

    // JSON serialization
    org::minima::utils::json::JSONObject toJson() const;
    void fromJson(const org::minima::utils::json::JSONObject& json);

    // Path management
    void addHopToPath(const InetSocketAddress& address);
    void addHopToPath(const InetSocketAddress* address); // throws if nullptr

    // Returns pointer to previous node in path relative to thisAddress, or nullptr if none
    const InetSocketAddress* getPreviousNode(const InetSocketAddress& thisAddress) const;

    // Getters/Setters
    const std::string& getTargetUidForNextHop() const;
    void setTargetUidForNextHop(const std::string& targetUidForNextHop);

    org::minima::objects::base::MiniData getSecret() const;
    void setSecret(const org::minima::objects::base::MiniData& secret);

    bool isWalkInLinks() const;
    void setWalkInLinks(bool walkInLinks);

    bool isJoiningWalk() const;
    void setJoiningWalk(bool joiningWalk);

    bool isClientWalk() const;
    void setClientWalk(bool clientWalk);

    int getAvailableNoneP2PConnectionSlots() const;
    void setAvailableNoneP2PConnectionSlots(int availableNoneP2PConnectionSlots);

    bool isReturning() const;
    void setReturning(bool returning);

    const std::vector<InetSocketAddress>& getPathTaken() const;
    void setPathTaken(const std::vector<InetSocketAddress>& pathTaken);

private:
    std::string m_targetUidForNextHop;
    bool m_walkInLinks{false};
    bool m_isJoiningWalk{false};
    bool m_isClientWalk{false};
    int m_availableNoneP2PConnectionSlots{0};
    bool m_isReturning{false};
    std::vector<InetSocketAddress> m_pathTaken;
    org::minima::objects::base::MiniData m_secret{org::minima::objects::base::MiniData::getRandomData(8)};
};

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org