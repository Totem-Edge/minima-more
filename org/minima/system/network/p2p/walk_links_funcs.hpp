#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/utils/messages/message.hpp"

namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOClientInfo;
}}}}}

namespace org { namespace minima { namespace system { namespace network { namespace p2p {
class P2PState;
class P2PManager;
class P2PFunctions;
namespace messages {
class P2PWalkLinks;
struct InetSocketAddress; // defined in inet_socket_address_i_o.hpp
class P2PDoSwap;
}
}}}}}

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

class WalkLinksFuncs {
public:
    // Type aliases for brevity
    using InetSocketAddress = org::minima::system::network::p2p::messages::InetSocketAddress;
    using Message = org::minima::utils::messages::Message;
    using P2PWalkLinks = org::minima::system::network::p2p::messages::P2PWalkLinks;
    using NIOClientInfo = org::minima::system::network::minima::NIOClientInfo;
    using P2PState = org::minima::system::network::p2p::P2PState;

    // Removes all elements in list B from the values in hashmap A, returning remaining addresses
    static std::vector<InetSocketAddress> removeIPsInBFromA(
        const std::unordered_map<std::string, InetSocketAddress>& a,
        const std::vector<InetSocketAddress>& b);

    // OutLinks walk message processing
    static std::unique_ptr<Message> onOutLinkWalkMsg(
        P2PState& state,
        P2PWalkLinks& p2pWalkLinks,
        const NIOClientInfo& clientMsgIsFrom,
        int tgtNumLinks,
        const std::vector<NIOClientInfo>& allClients);

    // Create a message to send the walk onto the next hop (or null if no client exists)
    static std::unique_ptr<Message> createNextHopMsg(
        const InetSocketAddress& destinationAddress,
        const P2PWalkLinks& p2pWalkLinks,
        P2PState& state);

    // Generate a DoSwap message from a walk (or null)
    static std::unique_ptr<Message> generateDoSwapMessageFromWalk(
        P2PState& state,
        const P2PWalkLinks& msgWalkLinks,
        int tgtNumLinks);

    // Get the address for a UID by searching all link maps (nullopt if not found)
    static std::optional<InetSocketAddress> getAddressFromUID(P2PState& state, const std::string& uid);

    // InLinks walk message processing
    static std::unique_ptr<Message> onInLinkWalkMsg(
        P2PState& state,
        P2PWalkLinks& p2pWalkLinks,
        const NIOClientInfo& clientMsgIsFrom,
        const std::vector<NIOClientInfo>& allClients);

    // Generate the message required to send the walk back down the path
    static std::unique_ptr<Message> onWalkLinkResponseMsg(P2PState& state, const P2PWalkLinks& p2pWalkLinks);

    // Process a returned InLink walk at origin (returns connect messages list - currently empty, mirrors Java)
    static std::vector<Message> onReturnedWalkMsg(P2PState& state, P2PWalkLinks& walkLinks, int tgtNumOutLinks);

    // Process a returned Load-Balance walk message
    static std::vector<Message> onReturnedLoadBalanceWalkMsg(P2PState& state, const P2PWalkLinks& msg);

    // Generate DoSwap messages for load balancing
    static std::vector<Message> genLoadBalanceDoSwaps(P2PState& state, const InetSocketAddress& address, int maxClientsCanReceive);

private:
    WalkLinksFuncs() = delete;
};

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org