#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <random>

#include "org/minima/utils/messages/message_processor.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"

// Forward declarations to satisfy signatures (Rule 10)
namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOClientInfo;
class NIOClient;
} } } } }

namespace org { namespace minima { namespace system { namespace network { namespace p2p {
class P2PState;
class P2PPeersChecker;
} } } } }

namespace org { namespace minima { namespace system { namespace network { namespace p2p { namespace messages {
class P2PWalkLinks;
} } } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

class P2PManager : public org::minima::utils::messages::MessageProcessor {
public:
    // Reset functions - for SSH Tunnel
    inline static const char* P2P_RESET           = "P2P_RESET";
    inline static const char* P2P_RANDOM_CONNECT  = "P2P_RANDOM_CONNECT";

    // Loop message repeated every so often
    inline static const char* P2P_LOOP                   = "P2P_LOOP";
    inline static const char* P2P_ASSESS_CONNECTIVITY    = "P2P_ASSESS_CONNECTIVITY";
    inline static const char* P2P_SEND_MSG               = "P2P_SEND_MSG";
    inline static const char* P2P_SEND_MSG_TO_ALL        = "P2P_SEND_MSG_TO_ALL";
    inline static const char* P2P_SEND_CONNECT           = "P2P_SEND_CONNECT";
    inline static const char* P2P_SEND_DISCONNECT        = "P2P_SEND_DISCONNECT";

    inline static const char* P2P_ADD_PEER    = "P2P_ADD_PEER";
    inline static const char* P2P_REMOVE_PEER = "P2P_REMOVE_PEER";
    inline static const char* P2P_SAVE_DATA   = "P2P_SAVE_DATA";

    // Message map literal keys
    inline static const char* ADDRESS_LITERAL = "address";

    // Health check
    inline static const char* P2P_HEALTH_CHECK = "P2P_HEALTH_CHECK";
    

    P2PManager();

    // PIMPL-FIX for unique_ptr to forward-declared types
    virtual ~P2PManager();
    P2PManager(P2PManager&&) noexcept = delete;
    P2PManager& operator=(P2PManager&&) noexcept = delete;
    P2PManager(const P2PManager&) = delete;
    P2PManager& operator=(const P2PManager&) = delete;

    org::minima::utils::json::JSONObject getStatus(bool fullDetails);

    // Accessors mirrored from Java
    org::minima::system::network::p2p::P2PPeersChecker* getPeersChecker();
    std::vector<org::minima::system::network::p2p::messages::InetSocketAddress> getPeersCopy();
    std::string getP2PAddress();
    float getClients();

    // Public helpers
    void updateP2PPeersList();
    void shutdown();
    bool haveAnyPeers();

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    using InetSocketAddress = org::minima::system::network::p2p::messages::InetSocketAddress;

    // Internal stages
    std::vector<org::minima::utils::messages::Message> init(org::minima::system::network::p2p::P2PState& state);
    void doDiscoveryPing();

    static std::vector<org::minima::utils::messages::Message> assessConnectivity(org::minima::system::network::p2p::P2PState& state);
    std::vector<org::minima::utils::messages::Message> processJsonMessages(org::minima::utils::messages::Message& zMessage,
                                                                           org::minima::system::network::p2p::P2PState& state);
    std::vector<org::minima::utils::messages::Message> processLoop(org::minima::system::network::p2p::P2PState& state);

    static std::vector<org::minima::utils::messages::Message> processWalkLinksMsg(org::minima::utils::json::JSONObject& zMessage,
                                                                                  org::minima::system::network::minima::NIOClientInfo& clientInfo,
                                                                                  org::minima::system::network::p2p::P2PState& state);
    static std::vector<org::minima::utils::messages::Message> processReturningMessage(org::minima::system::network::p2p::messages::P2PWalkLinks p2pWalkLinks,
                                                                                      org::minima::system::network::p2p::P2PState& state);
    static org::minima::utils::messages::Message processOutgoingWalkMessage(org::minima::system::network::p2p::messages::P2PWalkLinks p2pWalkLinks,
                                                                            org::minima::system::network::minima::NIOClientInfo& clientInfo,
                                                                            org::minima::system::network::p2p::P2PState& state);
    static std::vector<org::minima::utils::messages::Message> connect(org::minima::utils::messages::Message& zMessage,
                                                                      org::minima::system::network::p2p::P2PState& state);


    void sendMessages(const std::vector<org::minima::utils::messages::Message>& sendMessages);

    // Helpers
    static bool inetAddrEqual(const InetSocketAddress& a, const InetSocketAddress& b);

    // Members
    std::unique_ptr<org::minima::system::network::p2p::P2PState> mState;
    std::unique_ptr<org::minima::system::network::p2p::P2PPeersChecker> mPeersChecker;

    // State variables
    int mInitialPeersListNum  = 0;
    long mP2PHealthCheckTimer = static_cast<long>(1000) * 60 * 10;

    std::vector<std::string> mExcludeFromClear;

    long mLastNotifyNoPeers = 0;
    long mNotifyNoPeersTimer = static_cast<long>(1000) * 60 * 3;

    // RNG
    std::mt19937 mRng;
};

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org