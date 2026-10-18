#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>

// Base class must be included in header (inheritance rule)
#include "org/minima/utils/messages/message_stack.hpp"

// Forward declarations for project classes used in members/signatures
namespace org { namespace minima { namespace utils { class MinimaLogger; } } }
namespace org { namespace minima { namespace database { class MinimaDB; } } }

namespace org {
namespace minima {
namespace utils {
namespace messages {

class Message;
class TimerMessage;
class TimerProcessor;

class MessageProcessor : public MessageStack {
public:
    explicit MessageProcessor(const std::string& zName);
    virtual ~MessageProcessor();

    // Non-copyable, non-movable (owns a thread)
    MessageProcessor(const MessageProcessor&) = delete;
    MessageProcessor& operator=(const MessageProcessor&) = delete;
    MessageProcessor(MessageProcessor&&) = delete;
    MessageProcessor& operator=(MessageProcessor&&) = delete;

    // Public Java API equivalents
    std::string getName() const;

    static void setTrace(bool zTrace, const std::string& zTraceFilter);
    void setFullLogging(bool zLogON, const std::string& zTraceFilter);

    bool isTrace() const;
    std::string getTraceFilter() const;

    Message* getLastMessage() const;

    bool checkTraceFilter(const std::string& zMsg) const;

    bool isRunning() const;
    bool isShutdownComplete() const;

    void waitToShutDown();
    void stopMessageProcessor();

    // Post a timer message (shared ownership to mirror Java semantics)
    void PostTimerMessage(const std::shared_ptr<TimerMessage>& zMessage);

    // Runnable
    void run();

protected:
    // Start the worker thread explicitly after derived-class construction is complete.
    void startMessageProcessorThread();

    // Abstract processing method (must be implemented by derived classes)
    virtual void processMessage(Message& zMessage) = 0;

private:
    void setCurrentThreadName(const std::string& name);

    // Thread and state
    std::thread mMainThread;
    std::atomic<bool> mRunning;
    std::atomic<bool> mShutDownComplete;

    // Static tracing controls
    static std::atomic<bool> sTrace;
    static std::string sTraceFilter;

    // Name and last processed message
    std::string mName;
    std::shared_ptr<Message> mLastMessage;
};

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org