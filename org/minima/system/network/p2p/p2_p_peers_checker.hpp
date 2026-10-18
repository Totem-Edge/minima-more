#pragma once

#include <string>
#include <unordered_set>
#include <cstdint>
#include <random>
#include <functional> // for std::hash

// Base class include (inheritance)
#include "org/minima/utils/messages/message_processor.hpp"

// Use the same InetSocketAddress type as the rest of the P2P system so that
// P2PManager and P2PPeersChecker can exchange addresses in messages.
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"

// Forward declarations for project classes used in members/signatures
namespace org { namespace minima { namespace utils { namespace messages { class Message; class TimerMessage; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace objects { class Greeting; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; class TxPoWTree; } } } }
namespace org { namespace minima { namespace system { namespace network { namespace minima { class NIOManager; } } } } }
namespace org { namespace minima { namespace system { namespace params { class GeneralParams; class GlobalParams; } } } }
namespace org { namespace minima { namespace utils { class MinimaLogger; } } }
namespace org { namespace minima { namespace system { namespace network { namespace p2p { class P2PManager; class P2PFunctions; } } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

// Alias the P2P messages InetSocketAddress into the p2p namespace so the rest
// of this file matches the Java usage of a single InetSocketAddress type.
using InetSocketAddress = org::minima::system::network::p2p::messages::InetSocketAddress;

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org

//
// Hash specialization for messages::InetSocketAddress so it can be stored in
// unordered_sets.
//
namespace std {
template<>
struct hash<org::minima::system::network::p2p::messages::InetSocketAddress> {
    size_t operator()(const org::minima::system::network::p2p::messages::InetSocketAddress& a) const noexcept {
        std::hash<std::string> hs;
        std::hash<int> hi;
        // Simple combine
        size_t hhost = hs(a.getAddress().getHostAddress());
        size_t hport = hi(a.getPort());
        return hhost ^ (hport + 0x9e3779b9 + (hhost<<6) + (hhost>>2));
    }
};
} // namespace std

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

class P2PPeersChecker : public org::minima::utils::messages::MessageProcessor {
public:
    inline static const char* PEERS_INIT          = "PEERS_INIT";
    inline static const char* PEERS_ADDPEERS      = "PEERS_ADDPEERS";
    inline static const char* PEERS_CHECKPEERS    = "PEERS_CHECKPEERS";
    inline static const char* PEERS_LOOP          = "PEERS_LOOP";
    inline static const char* PEERS_FORCEFULLCHECK= "PEERS_FORCEFULLCHECK";

    static int MAX_VERIFIED_PEERS;

    explicit P2PPeersChecker(org::minima::system::network::p2p::P2PManager* manager);

    // Accessors for peer sets
    const std::unordered_set<InetSocketAddress>& getUnverifiedPeers() const;
    const std::unordered_set<InetSocketAddress>& getVerifiedPeers() const;
    std::unordered_set<InetSocketAddress>& getVerifiedPeers();

    // Public API equivalents
    bool haveAnyPeers() const;
    void checkUnverifiedPeer(const InetSocketAddress& zAddress);

    virtual ~P2PPeersChecker() = default;

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    // Remove one random element from set; returns true if removed and sets out parameter
    bool removeRandomItem(std::unordered_set<InetSocketAddress>& zSet, InetSocketAddress& outRemoved);

    // Sets of peers
    std::unordered_set<InetSocketAddress> m_unverifiedPeers;
    std::unordered_set<InetSocketAddress> m_verifiedPeers;

    // Back-reference to manager (non-owning)
    org::minima::system::network::p2p::P2PManager* m_p2pManager {nullptr};

    // Loop timer (6 hours)
    std::int64_t m_PEERS_LOOP_TIMER = static_cast<std::int64_t>(1000) * 60 * 60 * 6;

    // RNG
    std::mt19937 m_rng;
};

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
