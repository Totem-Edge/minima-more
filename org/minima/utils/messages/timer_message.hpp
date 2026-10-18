#pragma once

#include <cstdint>
#include <string>

#include "org/minima/utils/messages/message.hpp"

namespace org {
namespace minima {
namespace utils {
namespace messages {

// Forward declaration to avoid heavy includes; non-owning pointer used.
class MessageProcessor;

class TimerMessage : public Message {
public:
    // Construct with a delay (milliseconds) and a message type.
    TimerMessage(std::int64_t zDelay, const std::string& zMessageType);

    // Construct with a delay (milliseconds) and an existing Message whose
    // type and contents are aliased (as in the Java version).
    // Note: non-const to match Message::getAllContents() non-const and alias semantics.
    TimerMessage(std::int64_t zDelay, Message& zMessage);

    // Associate the originating processor (non-owning).
    void setProcessor(MessageProcessor* zProcessor);
    MessageProcessor* getProcessor() const;

    // The absolute timer value in milliseconds since epoch when this should trigger.
    std::int64_t getTimer() const;

    // String representation (hides base, which is non-virtual in provided header).
    std::string toString() const;

private:
    std::int64_t      mTimer{0};
    MessageProcessor* mProcessor{nullptr};
};

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org