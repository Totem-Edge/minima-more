#include "org/minima/utils/messages/message_processor.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <iostream>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/database/minima_d_b.hpp"

// Full headers for dependent classes (required per special instructions)
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/timer_message.hpp"
#include "org/minima/utils/messages/timer_processor.hpp"
#include "org/minima/utils/messages/message_stack.hpp"

#if defined(__APPLE__) || defined(__linux__)
  #include <pthread.h>
#endif

namespace org {
namespace minima {
namespace utils {
namespace messages {

// Static members
std::atomic<bool> MessageProcessor::sTrace{false};
std::string MessageProcessor::sTraceFilter{""};

MessageProcessor::MessageProcessor(const std::string& zName)
    : mMainThread()
    , mRunning(true)
    , mShutDownComplete(false)
    , mName(zName)
    , mLastMessage(nullptr) {
}

void MessageProcessor::startMessageProcessorThread() {
    mMainThread = std::thread(&MessageProcessor::run, this);
}

MessageProcessor::~MessageProcessor() {
    // Ensure the thread is stopped cleanly
    mRunning.store(false);
    try {
        // Wake up if the thread is waiting/sleeping
        notifyLock();
    } catch (...) {
        // ignore
    }
    if (mMainThread.joinable()) {
        mMainThread.join();
    }
}

std::string MessageProcessor::getName() const {
    return mName;
}

void MessageProcessor::setTrace(bool zTrace, const std::string& zTraceFilter) {
    sTrace.store(zTrace);
    sTraceFilter = zTraceFilter;
}

void MessageProcessor::setFullLogging(bool zLogON, const std::string& zTraceFilter) {
    sTrace.store(zLogON);
    sTraceFilter = zTraceFilter;
}

bool MessageProcessor::isTrace() const {
    return sTrace.load();
}

std::string MessageProcessor::getTraceFilter() const {
    return sTraceFilter;
}

Message* MessageProcessor::getLastMessage() const {
    return mLastMessage.get();
}

bool MessageProcessor::checkTraceFilter(const std::string& zMsg) const {
    if (!sTrace.load()) {
        return false;
    }
    std::string lowerMsg = zMsg;
    std::string lowerFilter = sTraceFilter;
    std::transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return lowerMsg.find(lowerFilter) != std::string::npos;
}

bool MessageProcessor::isRunning() const {
    return mRunning.load();
}

bool MessageProcessor::isShutdownComplete() const {
    return mShutDownComplete.load();
}

void MessageProcessor::waitToShutDown() {
    using namespace std::chrono;
    long timewaited = 0;
    while (!isShutdownComplete()) {
        std::this_thread::sleep_for(milliseconds(250));
        timewaited += 250;
        if (timewaited > 15000) {
            org::minima::utils::MinimaLogger::log(
                "Failed to shutdown in 15 secs for " + mName);
            // Hard shutdown
            mShutDownComplete.store(true);
            mRunning.store(false);

            // Wake via notify (no standard interrupt in C++)
            try { notifyLock(); } catch (...) {}

            return;
        }
    }
}

void MessageProcessor::stopMessageProcessor() {
    mRunning.store(false);
    // Wake it up if it is locked..
    notifyLock();
}

void MessageProcessor::PostTimerMessage(const std::shared_ptr<TimerMessage>& zMessage) {
    if (!zMessage) {
        // Mirror Java's behavior of logging null message attempts via TimerProcessor
        org::minima::utils::MinimaLogger::log("NULL TIMER Message attempt:");
        return;
    }
    // Set this as the processor
    zMessage->setProcessor(this);
    // Post it on the TimerProcessor
    TimerProcessor::getTimerProcessor()->PostMessage(zMessage);
}

void MessageProcessor::setCurrentThreadName(const std::string& name) {
#if defined(__APPLE__)
    // macOS sets name on current thread only
    pthread_setname_np(name.substr(0, 63).c_str());
#elif defined(__linux__)
    // On Linux name length limit is typically 16 including NUL
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#else
    (void)name; // no-op on other platforms
#endif
}

void MessageProcessor::run() {
    // Ensure the thread has the desired name for platforms that require setting from current thread
    setCurrentThreadName(mName);

    if (sTrace.load()) {
        org::minima::utils::MinimaLogger::log("[" + mName + "] (stack:" + std::to_string(getSize()) + ") START", false);
    }

    using clock = std::chrono::steady_clock;

    while (mRunning.load()) {
        // Check for a valid message
        std::shared_ptr<Message> msg = getNextMessage();

        // Cycle through available messages
        while (msg != nullptr && mRunning.load()) {
            try {
                // Check for trace
                std::string tracemsg = msg->toString();

                // Are we logging?
                clock::time_point timenow;
                bool dotrace = checkTraceFilter(tracemsg);
                if (dotrace) {
                    timenow = clock::now();
                }

                // Store in case sys check
                mLastMessage = msg;

                // Process Message
                processMessage(*msg);

                if (dotrace) {
                    auto timediff = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - timenow).count();
                    org::minima::utils::MinimaLogger::log(
                        "TRACE > [" + mName + "] (stack:" + std::to_string(getSize()) + ") process_time_milli:" +
                        std::to_string(timediff) + " \t" + msg->toString(), false);
                }
            } catch (const std::exception& exc) {
                org::minima::utils::MinimaLogger::log("MESSAGE PROCESSING ERROR @ " + msg->getMessageType());
                org::minima::utils::MinimaLogger::log(exc);
            } catch (...) {
                // Serious, non-standard error
                org::minima::utils::MinimaLogger::log("**SERIOUS ERROR " + msg->getMessageType() + " (unknown non-std exception)");
            }

            // Make sure the write lock is released (finally)
            try {
                org::minima::database::MinimaDB::getDB()->safeReleaseReadWriteLock();
            } catch (...) {
                // ignore any issues releasing lock
            }

            // Are there more messages..
            msg = getNextMessage();
        }

        if (mRunning.load() && !isNextMessage()) {
            try {
                std::unique_lock<std::mutex> lock(mLockMutex);
                mLockCV.wait_for(lock, std::chrono::seconds(1),
                    [this]() { return !mRunning.load() || isNextMessage(); });
            } catch (const std::exception& exc) {
                org::minima::utils::MinimaLogger::log("MESSAGE PROCESSOR wait lock exception @ " + mName + ": " + std::string(exc.what()));
            } catch (...) {
                org::minima::utils::MinimaLogger::log("MESSAGE PROCESSOR wait lock unknown exception @ " + mName);
            }
        }
    }

    if (sTrace.load()) {
        org::minima::utils::MinimaLogger::log("[" + mName + "] (stack:" + std::to_string(getSize()) + ") SHUTDOWN", false);
    }

    // All done..
    mRunning.store(false);
    mShutDownComplete.store(true);
}

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org