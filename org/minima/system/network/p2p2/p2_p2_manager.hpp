#pragma once

#include <string>
#include <cstdint>

#include "org/minima/utils/messages/message_processor.hpp"

// Forward declarations (avoid heavy includes in header)
namespace org { namespace minima { namespace utils { namespace messages { class Message; } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p2 {

class P2P2Manager : public org::minima::utils::messages::MessageProcessor {
public:
    // Message type constants (mirror Java static strings)
    static constexpr const char* P2P2_INIT      = "P2P2_INIT";
    static constexpr const char* P2P2_SHUTDOWN  = "P2P2_SHUTDOWN";
    static constexpr const char* P2P2_FASTLOOP  = "P2P2_FAST_LOOP";
    static constexpr const char* P2P2_SLOWLOOP  = "P2P2_SLOW_LOOP";

    // Loop timers (milliseconds)
    std::int64_t P2P2_LOOP_TIMER       = 1000LL * 60LL * 5LL;      // 5 mins
    std::int64_t P2P2_LOOP_TIMER_SLOW  = 1000LL * 60LL * 60LL * 6; // 6 hours

    // Desired number of connections
    int NUMBER_DESIRED_CONNECTIONS = 3;

    P2P2Manager();
    virtual ~P2P2Manager() = default;

    // Shutdown sequence
    void shutdown();

    // Select a random peer from the known list (empty string if none)
    std::string getRandomPeerFromList();

    // PING 3 random hosts and connect to the one with the least connections (not implemented in Java)
    void checkWhichConnect();

    // Convert old P2P peers to new format (currently no-op as in Java)
    void convertOldP2P();

protected:
    // Message processing override
    void processMessage(org::minima::utils::messages::Message& zMessage) override;
};

} // namespace p2p2
} // namespace network
} // namespace system
} // namespace minima
} // namespace org