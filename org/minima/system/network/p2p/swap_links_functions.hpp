#pragma once

#include <string>
#include <vector>

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"

// Forward declarations for project types used by pointer/reference
namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOClient;
class NIOClientInfo;
}}}}}

namespace org { namespace minima { namespace system { namespace network { namespace p2p {
class P2PState;
}}}}}

namespace org { namespace minima { namespace system { namespace network { namespace p2p { namespace messages {
class P2PGreeting;
class P2PWalkLinks;
struct InetSocketAddress;
}}}}}}

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

class SwapLinksFunctions {
public:
    // Wraps a JSONObject into {"swap_links_p2p": data}
    static org::minima::utils::json::JSONObject wrapP2PMsg(const org::minima::utils::json::JSONObject& data);

    // On new connection: update none-P2P links, send greeting and possibly request IP
    static std::vector<org::minima::utils::messages::Message>
    onConnected(P2PState& state, bool incoming, org::minima::system::network::minima::NIOClient& info);

    // If overloaded with NoneP2P nodes, request load balancing via random walk
    static std::vector<org::minima::utils::messages::Message>
    onConnectedLoadBalanceRequest(P2PState& state,
                                  const std::vector<org::minima::system::network::minima::NIOClientInfo>& clients);

    // On disconnection: remove uid from all link sets
    static void onDisconnected(P2PState& state, const org::minima::utils::messages::Message& zMessage);

    // Process P2P greeting
    static bool processGreeting(P2PState& state,
                                const org::minima::system::network::p2p::messages::P2PGreeting& greeting,
                                const org::minima::system::network::minima::NIOClientInfo* client,
                                bool noconnect);

    // Request/Response IP helpers
    static org::minima::utils::json::JSONObject
    processRequestIPMsg(const org::minima::utils::json::JSONObject& swapLinksMsg, const std::string& host);

    static void processResponseIPMsg(P2PState& state, const org::minima::utils::json::JSONObject& swapLinksMsg);

    // Scale out-links / request in-links
    static std::vector<org::minima::utils::messages::Message>
    joinScaleOutLinks(P2PState& state, int targetNumLinks,
                      const std::vector<org::minima::system::network::minima::NIOClientInfo>& clients);

    static std::vector<org::minima::utils::messages::Message>
    requestInLinks(P2PState& state, int targetNumLinks,
                   const std::vector<org::minima::system::network::minima::NIOClientInfo>& clients);
};

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org