#include "org/minima/utils/messages/timer_processor.hpp"

#include <chrono>
#include <utility>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/messages/timer_message.hpp"

// Some headers reference ::Message; forward declare to satisfy them.
class Message;

#include "org/minima/utils/messages/message_processor.hpp"

#if (defined(__APPLE__) || (defined(__linux__) && defined(__GLIBC__))) && !defined(__ANDROID__)
  #include <execinfo.h>
  #include <cstdlib>
#endif


namespace org {
namespace minima {
namespace utils {
namespace messages {

std::unique_ptr<TimerProcessor> TimerProcessor::s_timerProcessor = nullptr;

static void log_current_stacktrace() {
#if (defined(__APPLE__) || (defined(__linux__) && defined(__GLIBC__))) && !defined(__ANDROID__)
    void* callstack[64];
    int frames = ::backtrace(callstack, 64);
    char** strs = ::backtrace_symbols(callstack, frames);
    if (strs) {
        for (int i = 0; i < frames; ++i) {
            org::minima::utils::MinimaLogger::log(std::string("  ") + strs[i]);
        }
        free(strs);
    }
#else
    org::minima::utils::MinimaLogger::log("Stack trace not available on this platform (musl/Windows).");
#endif
}


void TimerProcessor::createTimerProcessor() {
    // Replace existing instance safely; destructor will stop and join.
    s_timerProcessor.reset(new TimerProcessor());
}

TimerProcessor* TimerProcessor::getTimerProcessor() {
    return s_timerProcessor.get();
}

void TimerProcessor::stopTimerProcessor() {
    if (s_timerProcessor) {
        s_timerProcessor->stop();
    }
}

TimerProcessor::TimerProcessor()
    : mRunning(true)
    , mMainThread()
    , mTimerMessages()
    , mMessagesMutex()
    , mSleepMutex()
    , mSleepCv() {
    // Start the main processing thread
    mMainThread = std::thread(&TimerProcessor::run, this);
}

TimerProcessor::~TimerProcessor() {
    stop();
    if (mMainThread.joinable()) {
        mMainThread.join();
    }
}

void TimerProcessor::stop() {
    mRunning.store(false, std::memory_order_relaxed);
    // Wake up the sleeping thread (emulates Java interrupt)
    mSleepCv.notify_all();
}

void TimerProcessor::PostMessage(const std::shared_ptr<TimerMessage>& zMessage) {
    std::lock_guard<std::mutex> lk(mMessagesMutex);
    if (zMessage) {
        mTimerMessages.emplace_back(zMessage);
    } else {
        // Intermittent bug notice
        org::minima::utils::MinimaLogger::log("NULL TIMER Message attempt:");
        // Best-effort stack trace
        log_current_stacktrace();
    }
}

int TimerProcessor::getSize() const {
    std::lock_guard<std::mutex> lk(mMessagesMutex);
    return static_cast<int>(mTimerMessages.size());
}

void TimerProcessor::printAllMessages() {
    std::lock_guard<std::mutex> lk(mMessagesMutex);
    for (const auto& tm : mTimerMessages) {
        if (tm) {
            org::minima::utils::MinimaLogger::log(tm->toString(), false);
        } else {
            org::minima::utils::MinimaLogger::log("null TimerMessage in list", false);
        }
    }
}

int64_t TimerProcessor::currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void TimerProcessor::run() {
    while (mRunning.load(std::memory_order_relaxed)) {
        {
            std::lock_guard<std::mutex> lk(mMessagesMutex);

            // New list to store the ongoing timers
            std::vector<std::shared_ptr<TimerMessage>> newlist;
            newlist.reserve(mTimerMessages.size());

            // Current time
            int64_t time = currentTimeMillis();

            // Cycle through all the timers
            for (auto& tmPtr : mTimerMessages) {
                if (!tmPtr) {
                    org::minima::utils::MinimaLogger::log("Timer Message is NULL.. ?");
                    continue;
                }

                // Check the time
                if (tmPtr->getTimer() < time) {
                    // Who gets it
                    MessageProcessor* process = tmPtr->getProcessor();

                    // And post
                    if (process && process->isRunning()) {
                        // Cast to namespaced base Message and post
                        auto basePtr = std::static_pointer_cast<Message>(tmPtr);
                        process->PostMessage(basePtr);
                    } else {
                        org::minima::utils::MinimaLogger::log(
                            std::string("Timer Message NOT run as processor shutdown.. ") + tmPtr->toString());
                        // Do not requeue on shutdown (mirrors Java behavior)
                    }
                } else {
                    // Keep for next test
                    newlist.emplace_back(tmPtr);
                }
            }

            // Swap lists.
            mTimerMessages = std::move(newlist);
        }

        // Small sleep; emulate Java's interruptible sleep using condition_variable
        std::unique_lock<std::mutex> slk(mSleepMutex);
        mSleepCv.wait_for(slk, std::chrono::seconds(1), [this] { return !mRunning.load(std::memory_order_relaxed); });
    }
}

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org