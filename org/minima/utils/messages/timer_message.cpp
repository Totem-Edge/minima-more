#include "org/minima/utils/messages/timer_message.hpp"

#include <chrono>
#include <string>

namespace org {
namespace minima {
namespace utils {
namespace messages {

namespace {
inline std::int64_t currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
} // anonymous namespace

TimerMessage::TimerMessage(std::int64_t zDelay, const std::string& zMessageType)
    : Message(zMessageType),
      mTimer(currentTimeMillis() + zDelay),
      mProcessor(nullptr) {
}

TimerMessage::TimerMessage(std::int64_t zDelay, Message& zMessage)
    : Message(zMessage.getMessageType()),
      mTimer(currentTimeMillis() + zDelay),
      mProcessor(nullptr) {
    // Mirror Java: direct assignment to protected mContents from base.
    // This aliases the contents (shared_ptr) to emulate Java reference semantics.
    mContents = zMessage.getAllContents();
}

void TimerMessage::setProcessor(MessageProcessor* zProcessor) {
    mProcessor = zProcessor;
}

MessageProcessor* TimerMessage::getProcessor() const {
    return mProcessor;
}

std::int64_t TimerMessage::getTimer() const {
    return mTimer;
}

std::string TimerMessage::toString() const {
    const std::int64_t timenow = currentTimeMillis();
    const std::int64_t diff    = mTimer - timenow;

    // "Timer:<mTimer> Delay:<diff> milli Msg:<super.toString()>"
    return std::string("Timer:") + std::to_string(mTimer) +
           " Delay:" + std::to_string(diff) +
           " milli Msg:" + Message::toString();
}

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org