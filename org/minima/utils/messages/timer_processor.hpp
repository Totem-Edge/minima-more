#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <string>

namespace org { namespace minima { namespace utils { class MinimaLogger; } } }
namespace org { namespace minima { namespace utils { namespace messages {
    class TimerMessage;
    class MessageProcessor;
} } } }

namespace org {
namespace minima {
namespace utils {
namespace messages {

class TimerProcessor {
public:
    // Static lifecycle for the singleton processor
    static void createTimerProcessor();
    static TimerProcessor* getTimerProcessor();
    static void stopTimerProcessor();

    // Post a timed message (shared ownership, matches Java reference semantics)
    void PostMessage(const std::shared_ptr<TimerMessage>& zMessage);

    // Info utilities
    int getSize() const;
    void printAllMessages();

    // Destructor
    virtual ~TimerProcessor();

    // Delete copy and move
    TimerProcessor(const TimerProcessor&) = delete;
    TimerProcessor& operator=(const TimerProcessor&) = delete;
    TimerProcessor(TimerProcessor&&) noexcept = delete;
    TimerProcessor& operator=(TimerProcessor&&) noexcept = delete;

private:
    // Private constructor to mirror Java's private ctor
    TimerProcessor();

    // Thread main
    void run();

    // Stop the processor (non-joining)
    void stop();

    // Current time in milliseconds since epoch
    static int64_t currentTimeMillis();

    // Members
    std::atomic<bool> mRunning;
    std::thread mMainThread;

    // Timed messages storage (shared ownership)
    std::vector<std::shared_ptr<TimerMessage>> mTimerMessages;

    // Synchronization for mTimerMessages
    mutable std::mutex mMessagesMutex;

    // Sleep/wakeup mechanism to emulate Java interrupt()
    std::mutex mSleepMutex;
    std::condition_variable mSleepCv;

    // Singleton instance
    static std::unique_ptr<TimerProcessor> s_timerProcessor;
};

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org