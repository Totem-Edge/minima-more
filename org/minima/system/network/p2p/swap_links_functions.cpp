#include "org/minima/system/network/p2p/swap_links_functions.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/network/minima/n_i_o_client.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"
#include "org/minima/system/network/p2p/messages/p2_p_greeting.hpp"
#include "org/minima/system/network/p2p/messages/p2_p_walk_links.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"

// Full project headers presumed available in the full codebase
#include "org/minima/system/network/p2p/p2_p_state.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"
#include "org/minima/system/network/p2p/util_funcs.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_server.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

using org::minima::objects::base::MiniData;
using org::minima::utils::json::JSONObject;
using org::minima::utils::messages::Message;
using org::minima::system::network::minima::NIOClient;
using org::minima::system::network::minima::NIOClientInfo;
using org::minima::system::network::p2p::messages::P2PGreeting;
using org::minima::system::network::p2p::messages::P2PWalkLinks;
using InetSocketAddress = org::minima::system::network::p2p::messages::InetSocketAddress;

JSONObject SwapLinksFunctions::wrapP2PMsg(const JSONObject& data) {
    JSONObject msg;
    msg.put("swap_links_p2p", data);
    return msg;
}

std::vector<Message> SwapLinksFunctions::onConnected(P2PState& state, bool incoming, NIOClient& info) {
    std::vector<Message> msgs;

    std::string uid = info.getUID();
    bool sendMessages = true;

    if (incoming) {
        InetSocketAddress incomingAddress(info.getHost(), 0);
        
        bool alreadyConnected = false;
        for (const auto& pair : state.getNoneP2PLinks()) {
            // C++ replacement for map.containsValue(value)
            // Manual comparison as InetSocketAddress has no operator==
            if (pair.second.getAddress().getHostAddress() == incomingAddress.getAddress().getHostAddress() &&
                pair.second.getPort() == incomingAddress.getPort()) { 
                alreadyConnected = true;
                break;
            }
        }

        if (alreadyConnected) {
            msgs.emplace_back("P2P_SEND_DISCONNECT");
            msgs.back().addString("uid", uid);
            sendMessages = false;
        }
        // C++ replacement for map.put(key, value)
        state.getNoneP2PLinks()[uid] = InetSocketAddress(info.getHost(), 0);
    } else {
        // C++ replacement for map.put(key, value)
        state.getNoneP2PLinks()[uid] = InetSocketAddress(info.getHost(), info.getPort());
    }

    if (sendMessages) {
        P2PGreeting greeting(state);
        msgs.emplace_back("P2P_SEND_MSG");
        msgs.back().addString("uid", uid).addObject("json", greeting.toJson());

        if (!state.isHostSet()) {
            JSONObject requestIp;
            MiniData secret = MiniData::getRandomData(12);
            state.setIpReqSecret(secret);
            requestIp.put("req_ip", secret.toString());
            msgs.emplace_back("P2P_SEND_MSG");
            msgs.back().addString("uid", uid).addObject("json", requestIp);
        }
    }
    return msgs;
}

std::vector<Message> SwapLinksFunctions::onConnectedLoadBalanceRequest(
    P2PState& state, const std::vector<NIOClientInfo>& /*clients*/) {

    std::vector<Message> msgs;

    // C++ replacement for !object.empty() check; using isHostSet()
    const bool hasMyAddr = state.isHostSet();
    if (state.isAcceptingInLinks() && hasMyAddr &&
        static_cast<int>(state.getNotAcceptingConnP2PLinks().size()) > state.getMaxNumNoneP2PConnections() &&
        !state.getInLinks().empty()) { // C++ replacement for !map.isEmpty()

        // C++ replacement for map.values()
        std::vector<InetSocketAddress> inValues;
        for (const auto& pair : state.getInLinks()) {
            inValues.push_back(pair.second);
        }

        // FIX: selectRandomAddress returns a pointer
        const InetSocketAddress* nextHopPtr = UtilFuncs::selectRandomAddress(inValues);
        if (nextHopPtr != nullptr) {
            InetSocketAddress nextHop = *nextHopPtr; // Dereference the pointer to get the value
            auto minimaClient = UtilFuncs::getClientFromInetAddress(nextHop, state);
            if (minimaClient != nullptr) {
                P2PWalkLinks walkLinks(true, false, minimaClient->getUID());
                walkLinks.setClientWalk(true);

                int multipleOverMax = static_cast<int>(state.getInLinks().size()) / state.getMaxNumP2PConnections();
                for (int i = 0; i < multipleOverMax; i++) {
                    msgs.emplace_back("P2P_SEND_MSG");
                    msgs.back().addString("uid", minimaClient->getUID()).addObject("json", walkLinks.toJson());
                }
            }
        }
    }
    return msgs;
}

void SwapLinksFunctions::onDisconnected(P2PState& state, const Message& zMessage) {
    std::string uid = zMessage.getString("uid");

    // C++ replacement for map.remove(key)
    state.getInLinks().erase(uid);
    state.getNotAcceptingConnP2PLinks().erase(uid);
    state.getOutLinks().erase(uid);
    state.getNoneP2PLinks().erase(uid);
}

bool SwapLinksFunctions::processGreeting(P2PState& state,
                                         const P2PGreeting& greeting,
                                         const NIOClientInfo* client,
                                         bool noconnect) {

    if (client != nullptr) {
        std::string uid  = client->getUID();
        std::string host = client->getHost();
        int port         = greeting.getMyMinimaPort();
        InetSocketAddress minimaAddress(host, port);

        bool addtoknown = (host.find("127.0.0.1") == std::string::npos);
        (void)addtoknown;

        // C++ replacement for map.remove(key)
        state.getNoneP2PLinks().erase(uid);

        // The NIOClient has received a P2Pgreeting.. Check if NULL or Tests fail
        // This is the C++ equivalent of the missing Java code
        if (org::minima::system::Main::getInstance() != nullptr) {
            std::shared_ptr<org::minima::system::network::minima::NIOClient> nioclient = 
                org::minima::system::Main::getInstance()
                    ->getNIOManager()
                    .getNIOServer()
                    ->getClient(uid);
            if (nioclient != nullptr) {
                nioclient->setReceivedP2PGreeting();
            }
        }

        if (greeting.isAcceptingInLinks()) {
            if (client->isIncoming()) {
                // C++ replacement for map.put(key, value)
                state.getInLinks()[uid] = minimaAddress;
            } else {
                // C++ replacement for map.put(key, value)
                state.getOutLinks()[uid] = minimaAddress;
                if (static_cast<int>(state.getOutLinks().size()) > state.getMaxNumP2PConnections()) {
                    P2PFunctions::disconnect(uid);
                }
            }

            if (noconnect) {
                noconnect = false;
            }

            if (state.isDoingDiscoveryConnection()) {
                state.setDoingDiscoveryConnection(false);
                P2PFunctions::disconnect(uid);
            }
        } else {
            // C++ replacement for map.put(key, value)
            state.getNotAcceptingConnP2PLinks()[uid] = minimaAddress;
        }
    } else {
        P2PFunctions::log_debug(std::string("[-] ERROR Client is null when processing greeting: ")
                                + greeting.toJson().toString());
    }
    return noconnect;
}

JSONObject SwapLinksFunctions::processRequestIPMsg(const JSONObject& swapLinksMsg, const std::string& host) {
    MiniData secret(swapLinksMsg.getString("req_ip"));
    JSONObject responseMsg;
    JSONObject ipResponse;
    ipResponse.put("res_ip", host);
    ipResponse.put("secret", secret.toString());
    responseMsg.put("swap_links_p2p", ipResponse);
    return responseMsg;
}

void SwapLinksFunctions::processResponseIPMsg(P2PState& state, const JSONObject& swapLinksMsg) {
    MiniData secret(swapLinksMsg.getString("secret"));
    if (state.getIpReqSecret().isEqual(secret)) {
        std::string hostIP = swapLinksMsg.getString("res_ip");
        state.setMyMinimaAddress(hostIP);
        state.setHostSet(true);
        // C++ replacement for set.remove(value)
        state.getKnownPeers().erase(state.getMyMinimaAddress());
        P2PFunctions::log_debug(std::string("[+] Setting My IP: ") + hostIP);
    }
}

std::vector<Message> SwapLinksFunctions::joinScaleOutLinks(P2PState& state,
                                                           int targetNumLinks,
                                                           const std::vector<NIOClientInfo>& /*clients*/) {
    std::vector<Message> sendMsgs;
    if (static_cast<int>(state.getOutLinks().size()) < targetNumLinks) {
        P2PFunctions::log_debug("Attempting to scale outlinks");

        // C++ replacement for map.values()
        std::vector<InetSocketAddress> outValues;
        for (const auto& pair : state.getOutLinks()) {
            outValues.push_back(pair.second);
        }

        // FIX: selectRandomAddress returns a pointer
        const InetSocketAddress* nextHopPtr = UtilFuncs::selectRandomAddress(outValues);
        if (nextHopPtr != nullptr) {
            InetSocketAddress nextHop = *nextHopPtr; // Dereference the pointer
            auto minimaClient = UtilFuncs::getClientFromInetAddress(nextHop, state);
            if (minimaClient != nullptr) {
                P2PWalkLinks walkLinksMsg(true, true, minimaClient->getUID());
                sendMsgs.emplace_back("P2P_SEND_MSG");
                sendMsgs.back().addString("uid", minimaClient->getUID()).addObject("json", walkLinksMsg.toJson());
            }
        }
    }
    return sendMsgs;
}

std::vector<Message> SwapLinksFunctions::requestInLinks(P2PState& state,
                                                        int targetNumLinks,
                                                        const std::vector<NIOClientInfo>& /*clients*/) {
    std::vector<Message> sendMsgs;
    if (state.isAcceptingInLinks() && static_cast<int>(state.getInLinks().size()) < targetNumLinks) {
        P2PFunctions::log_debug("Attempting to scale inlinks");
        
        // C++ replacement for map.values()
        std::vector<InetSocketAddress> outValues;
        for (const auto& pair : state.getOutLinks()) {
            outValues.push_back(pair.second);
        }

        // FIX: selectRandomAddress returns a pointer
        const InetSocketAddress* nextHopPtr = UtilFuncs::selectRandomAddress(outValues);
        if (nextHopPtr != nullptr) {
            InetSocketAddress nextHop = *nextHopPtr; // Dereference the pointer
            auto minimaClient = UtilFuncs::getClientFromInetAddress(nextHop, state);
            if (minimaClient != nullptr) {
                P2PWalkLinks walkLinksMsg(true, true, minimaClient->getUID());
                sendMsgs.emplace_back("P2P_SEND_MSG");
                sendMsgs.back().addString("uid", minimaClient->getUID()).addObject("json", walkLinksMsg.toJson());
            }
        }
    }
    return sendMsgs;
}

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
