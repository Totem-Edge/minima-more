#include "org/minima/system/network/p2p/walk_links_funcs.hpp"

#include <algorithm>
#include <random>
#include <iterator>
#include <type_traits>
#include <unordered_map>

#include "org/minima/system/network/p2p/messages/p2_p_walk_links.hpp"
#include "org/minima/system/network/p2p/messages/p2_p_do_swap.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/system/network/p2p/p2_p_state.hpp"
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"

#ifdef _WIN32
// No Windows-specific behavior needed here
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

namespace {

using InetSocketAddress = org::minima::system::network::p2p::messages::InetSocketAddress;
using Message = org::minima::utils::messages::Message;
using P2PWalkLinks = org::minima::system::network::p2p::messages::P2PWalkLinks;
using MiniData = org::minima::objects::base::MiniData;

// Random engine similar to Java's Random
std::mt19937& rng() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

// Detection traits for various InetSocketAddress host/port access patterns
template<typename T, typename = void>
struct has_getHostString : std::false_type {};
template<typename T>
struct has_getHostString<T, std::void_t<decltype(std::declval<const T&>().getHostString())>> : std::true_type {};

template<typename T, typename = void>
struct has_member_m_host : std::false_type {};
template<typename T>
struct has_member_m_host<T, std::void_t<decltype(std::declval<const T&>().m_host)>> : std::true_type {};

template<typename T, typename = void>
struct has_member_host : std::false_type {};
template<typename T>
struct has_member_host<T, std::void_t<decltype(std::declval<const T&>().host)>> : std::true_type {};

template<typename T, typename = void>
struct has_getPort : std::false_type {};
template<typename T>
struct has_getPort<T, std::void_t<decltype(std::declval<const T&>().getPort())>> : std::true_type {};

template<typename T, typename = void>
struct has_member_m_port : std::false_type {};
template<typename T>
struct has_member_m_port<T, std::void_t<decltype(std::declval<const T&>().m_port)>> : std::true_type {};

template<typename T, typename = void>
struct has_member_port : std::false_type {};
template<typename T>
struct has_member_port<T, std::void_t<decltype(std::declval<const T&>().port)>> : std::true_type {};

template<typename T>
std::string host_of(const T& x) {
    if constexpr (has_getHostString<T>::value) {
        return x.getHostString();
    } else if constexpr (has_member_m_host<T>::value) {
        return x.m_host;
    } else if constexpr (has_member_host<T>::value) {
        return x.host;
    } else {
        return std::string();
    }
}

template<typename T>
int port_of(const T& x) {
    if constexpr (has_getPort<T>::value) {
        return x.getPort();
    } else if constexpr (has_member_m_port<T>::value) {
        return x.m_port;
    } else if constexpr (has_member_port<T>::value) {
        return x.port;
    } else {
        return 0;
    }
}

bool addressesEqual(const InetSocketAddress& a, const InetSocketAddress& b) {
    return host_of(a) == host_of(b) && port_of(a) == port_of(b);
}

std::optional<InetSocketAddress> selectRandomAddress(const std::vector<InetSocketAddress>& addrs) {
    if (addrs.empty()) {
        return std::nullopt;
    }
    std::uniform_int_distribution<std::size_t> dist(0, addrs.size() - 1);
    return addrs[dist(rng())];
}

// Search all link maps to find UID for a given address
std::optional<std::string> findUIDByAddress(P2PState& state, const InetSocketAddress& addr) {
    const auto& inMap   = state.getInLinks();
    const auto& outMap  = state.getOutLinks();
    const auto& naMap   = state.getNotAcceptingConnP2PLinks();
    const auto& noneMap = state.getNoneP2PLinks();

    auto search = [&](const std::unordered_map<std::string, InetSocketAddress>& mp) -> std::optional<std::string> {
        for (const auto& kv : mp) {
            if (addressesEqual(kv.second, addr)) {
                return kv.first;
            }
        }
        return std::nullopt;
    };

    if (auto uid = search(inMap))   return uid;
    if (auto uid = search(outMap))  return uid;
    if (auto uid = search(naMap))   return uid;
    if (auto uid = search(noneMap)) return uid;

    return std::nullopt;
}

} // anonymous namespace

std::vector<WalkLinksFuncs::InetSocketAddress> WalkLinksFuncs::removeIPsInBFromA(
    const std::unordered_map<std::string, InetSocketAddress>& a,
    const std::vector<InetSocketAddress>& b) {

    std::vector<InetSocketAddress> out;
    out.reserve(a.size());

    for (const auto& kv : a) {
        const auto& val = kv.second;
        bool found = false;
        for (const auto& bb : b) {
            if (addressesEqual(val, bb)) {
                found = true;
                break;
            }
        }
        if (!found) {
            out.push_back(val);
        }
    }
    return out;
}

std::unique_ptr<WalkLinksFuncs::Message> WalkLinksFuncs::onOutLinkWalkMsg(
    P2PState& state,
    P2PWalkLinks& p2pWalkLinks,
    const NIOClientInfo& clientMsgIsFrom,
    int tgtNumLinks,
    const std::vector<NIOClientInfo>& /*allClients*/) {

    // Add the previous address to the list
    std::optional<InetSocketAddress> prev = getAddressFromUID(state, clientMsgIsFrom.getUID());
    if (prev.has_value()) {
        p2pWalkLinks.addHopToPath(prev.value());
    } else {
        // Mirror potential Java NPE path by calling nullptr overload (will throw)
        p2pWalkLinks.addHopToPath(static_cast<const InetSocketAddress*>(nullptr));
    }

    // Filter OutLinks not already in path
    std::vector<InetSocketAddress> filteredOutLinks = removeIPsInBFromA(state.getOutLinks(), p2pWalkLinks.getPathTaken());

    // Random next hop
    auto nextHop = selectRandomAddress(filteredOutLinks);

    if (p2pWalkLinks.getPathTaken().size() < 9 && nextHop.has_value()) {
        return createNextHopMsg(nextHop.value(), p2pWalkLinks, state);
    } else {
        return generateDoSwapMessageFromWalk(state, p2pWalkLinks, tgtNumLinks);
    }
}

std::unique_ptr<WalkLinksFuncs::Message> WalkLinksFuncs::createNextHopMsg(
    const InetSocketAddress& destinationAddress,
    const P2PWalkLinks& p2pWalkLinks,
    P2PState& state) {

    // Look up the UID for the destination address
    auto uid = findUIDByAddress(state, destinationAddress);
    if (uid) {
        auto ret = std::make_unique<Message>(org::minima::system::network::p2p::P2PManager::P2P_SEND_MSG);
        ret->addString("uid", *uid).addObject("json", p2pWalkLinks.toJson());
        return ret;
    } else {
        org::minima::utils::MinimaLogger::log("[-] P2P_WALK_LINKS NIOClientInfo for " + host_of(destinationAddress) + ":" + std::to_string(port_of(destinationAddress)) + " does not exist");
        return nullptr;
    }
}

std::unique_ptr<WalkLinksFuncs::Message> WalkLinksFuncs::generateDoSwapMessageFromWalk(
    P2PState& state,
    const P2PWalkLinks& msgWalkLinks,
    int tgtNumLinks) {

    std::unique_ptr<Message> msg;

    const auto& inlinks = state.getInLinks();
    if (inlinks.size() > static_cast<std::size_t>(tgtNumLinks)) {
        // Collect UIDs
        std::vector<std::string> uids;
        uids.reserve(inlinks.size());
        for (const auto& kv : inlinks) {
            uids.push_back(kv.first);
        }

        // Java: random.nextInt(uids.size() - 1) -> excludes last element
        if (uids.size() <= 1) {
            return nullptr;
        }
        int boundExclusive = static_cast<int>(uids.size()) - 1;
        std::uniform_int_distribution<int> dist(0, boundExclusive - 1);
        int idx = dist(rng());
        const std::string& swappingPeerUID = uids[idx];

        // Target is the origin of the walk (first hop in path)
        const auto& path = msgWalkLinks.getPathTaken();
        if (path.empty()) {
            return nullptr;
        }

        // Convert messages::InetSocketAddress -> P2PDoSwap::InetSocketAddress
        org::minima::system::network::p2p::messages::P2PDoSwap::InetSocketAddress swapTarget(host_of(path[0]), port_of(path[0]));

        auto swapLink = org::minima::system::network::p2p::messages::P2PDoSwap(
            msgWalkLinks.getSecret(),
            swapTarget,
            swappingPeerUID
        );

        msg = std::make_unique<Message>(org::minima::system::network::p2p::P2PManager::P2P_SEND_MSG);
        msg->addString("uid", swappingPeerUID);
        msg->addObject("json", swapLink.toJson());
    }

    return msg;
}

std::optional<WalkLinksFuncs::InetSocketAddress> WalkLinksFuncs::getAddressFromUID(P2PState& state, const std::string& uid) {
    const auto& inL   = state.getInLinks();
    const auto& outL  = state.getOutLinks();
    const auto& naL   = state.getNotAcceptingConnP2PLinks();
    const auto& noneL = state.getNoneP2PLinks();

    auto findIn = [&](const std::unordered_map<std::string, InetSocketAddress>& mp) -> std::optional<InetSocketAddress> {
        auto it = mp.find(uid);
        if (it != mp.end()) {
            return it->second;
        }
        return std::nullopt;
    };

    if (auto a = findIn(inL))   return a;
    if (auto a = findIn(outL))  return a;
    if (auto a = findIn(naL))   return a;
    if (auto a = findIn(noneL)) return a;

    org::minima::utils::MinimaLogger::log("No client found for: " + uid);
    return std::nullopt;
}

std::unique_ptr<WalkLinksFuncs::Message> WalkLinksFuncs::onInLinkWalkMsg(
    P2PState& state,
    P2PWalkLinks& p2pWalkLinks,
    const NIOClientInfo& clientMsgIsFrom,
    const std::vector<NIOClientInfo>& /*allClients*/) {

    // Add the previous address to the list
    std::optional<InetSocketAddress> prev = getAddressFromUID(state, clientMsgIsFrom.getUID());
    if (prev.has_value()) {
        p2pWalkLinks.addHopToPath(prev.value());
    } else {
        p2pWalkLinks.addHopToPath(static_cast<const InetSocketAddress*>(nullptr));
    }

    // Filter InLinks not already in path
    std::vector<InetSocketAddress> filteredInLinks = removeIPsInBFromA(state.getInLinks(), p2pWalkLinks.getPathTaken());
    auto nextHop = selectRandomAddress(filteredInLinks);

    if (p2pWalkLinks.getPathTaken().size() < 9 && nextHop.has_value()) {
        return createNextHopMsg(nextHop.value(), p2pWalkLinks, state);
    } else {
        p2pWalkLinks.addHopToPath(state.getMyMinimaAddress());
        p2pWalkLinks.setReturning(true);
        if (p2pWalkLinks.isClientWalk()) {
            int available = state.getMaxNumNoneP2PConnections() - static_cast<int>(state.getNotAcceptingConnP2PLinks().size());
            p2pWalkLinks.setAvailableNoneP2PConnectionSlots(available);
        }
        return onWalkLinkResponseMsg(state, p2pWalkLinks);
    }
}

std::unique_ptr<WalkLinksFuncs::Message> WalkLinksFuncs::onWalkLinkResponseMsg(P2PState& state, const P2PWalkLinks& p2pWalkLinks) {
    std::unique_ptr<Message> retMsg;

    const InetSocketAddress* nextHop = p2pWalkLinks.getPreviousNode(state.getMyMinimaAddress());
    if (!nextHop) {
        return retMsg;
    }

    auto uid = findUIDByAddress(state, *nextHop);
    if (uid) {
        retMsg = std::make_unique<Message>(org::minima::system::network::p2p::P2PManager::P2P_SEND_MSG);
        retMsg->addString("uid", *uid).addObject("json", p2pWalkLinks.toJson());
    }
    return retMsg;
}

std::vector<WalkLinksFuncs::Message> WalkLinksFuncs::onReturnedWalkMsg(P2PState& state, P2PWalkLinks& walkLinks, int tgtNumOutLinks) {
    std::vector<Message> retMsg;

    if (state.getOutLinks().size() < static_cast<std::size_t>(tgtNumOutLinks)) {
        const auto& path = walkLinks.getPathTaken();
        if (!path.empty() && !(addressesEqual(path.back(), state.getMyMinimaAddress()))) {
            bool doConnect = false;
            // Attempt to find a connectable target as in Java
            while (!doConnect && walkLinks.getPathTaken().size() > 1) {
                auto currentPath = walkLinks.getPathTaken();
                InetSocketAddress connectTargetAddress = currentPath.back();

                org::minima::system::network::p2p::P2PFunctions::log_debug(
                    std::string("[+] Walk to scale out-links returned. Path: ") +
                    " ... " +
                    " Connecting to node: " +
                    host_of(connectTargetAddress) + ":" + std::to_string(port_of(connectTargetAddress))
                );

                doConnect = org::minima::system::network::p2p::P2PFunctions::checkConnect(
                    host_of(connectTargetAddress), port_of(connectTargetAddress));

                // Remove the checked target from path (mirror Java behavior)
                currentPath.pop_back();
                walkLinks.setPathTaken(currentPath);
            }
            // Java had the actual connect message commented out; we mirror by not adding any message.
        } else {
            org::minima::system::network::p2p::P2PFunctions::log_debug("[!] P2P_WALK_LINKS_RESPONSE: Not Connecting as returned own address");
        }
    } else {
        org::minima::system::network::p2p::P2PFunctions::log_debug("[!] P2P_WALK_LINKS_RESPONSE: Not Connecting already have max numLinks");
    }

    return retMsg;
}

std::vector<WalkLinksFuncs::Message> WalkLinksFuncs::onReturnedLoadBalanceWalkMsg(P2PState& state, const P2PWalkLinks& msg) {
    std::vector<Message> returnMessage;

    if (msg.getPathTaken().empty()) {
        return returnMessage;
    }

    InetSocketAddress connectTargetAddress = msg.getPathTaken().back();
    if (!addressesEqual(connectTargetAddress, state.getMyMinimaAddress())) {
        auto msgs = genLoadBalanceDoSwaps(state, connectTargetAddress, msg.getAvailableNoneP2PConnectionSlots());
        returnMessage.insert(returnMessage.end(), msgs.begin(), msgs.end());
    } else {
        org::minima::system::network::p2p::P2PFunctions::log_debug("[!] P2P_WALK_LINKS_RESPONSE: Not Connecting as returned own address");
    }

    return returnMessage;
}

std::vector<WalkLinksFuncs::Message> WalkLinksFuncs::genLoadBalanceDoSwaps(P2PState& state, const InetSocketAddress& address, int maxClientsCanReceive) {
    std::vector<Message> retMessages;

    const auto& naLinks = state.getNotAcceptingConnP2PLinks();
    if (naLinks.size() > static_cast<std::size_t>(state.getMaxNumNoneP2PConnections())) {
        int numSwaps = std::min(maxClientsCanReceive, state.getMaxNumNoneP2PConnections() / 2);

        // Snapshot of UIDs
        std::vector<std::string> nonP2PLinkUIDs;
        nonP2PLinkUIDs.reserve(naLinks.size());
        for (const auto& kv : naLinks) {
            nonP2PLinkUIDs.push_back(kv.first);
        }

        for (int i = 0; i < numSwaps && i < static_cast<int>(nonP2PLinkUIDs.size()); ++i) {
            // Convert target address type for P2PDoSwap
            org::minima::system::network::p2p::messages::P2PDoSwap::InetSocketAddress swapTarget(host_of(address), port_of(address));

            auto doSwapMg = org::minima::system::network::p2p::messages::P2PDoSwap(MiniData::getRandomData(8), swapTarget, nonP2PLinkUIDs[i]);
            Message m(org::minima::system::network::p2p::P2PManager::P2P_SEND_MSG);
            m.addString("uid", nonP2PLinkUIDs[i]).addObject("json", doSwapMg.toJson());
            retMessages.push_back(std::move(m));
        }
    }

    return retMessages;
}

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org