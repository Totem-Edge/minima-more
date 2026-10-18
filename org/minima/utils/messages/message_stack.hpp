#pragma once

#include <memory>
#include <deque>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>

namespace org { namespace minima { namespace utils { namespace messages {
class Message; // forward declaration
} } } }

namespace org {
namespace minima {
namespace utils {
namespace messages {

/**
 * Thread Safe Message Stack (C++17)
 * Functional equivalent of org.minima.utils.messages.MessageStack
 */
class MessageStack {
public:
    MessageStack();
    virtual ~MessageStack();

    // Non-copyable (mutex and synchronization primitives are not copyable)
    MessageStack(const MessageStack&) = delete;
    MessageStack& operator=(const MessageStack&) = delete;

    // Utility function to add a message given just the message type
    void PostMessage(const std::string& zMessage);

    // Add a Message onto the stack (thread safe)
    void PostMessage(const std::shared_ptr<Message>& zMessage);

    // Is there a next message?
    bool isNextMessage();

    // Get the size of the stack
    int getSize();

    // Clear ALL messages
    void clear();

    // Clear all except those whose message type is contained within zExclude
    void clearExcept(const std::vector<std::string>& zExclude);

    // Clear all except those whose message type contains the substring zExclude
    void clearExceptString(const std::string& zExclude);

    // Print out the stack - NO NOTIFY
    void printAllMessages();

protected:
    // Wake waiting threads
    void notifyLock();

    // Get the first message on the stack, if there is one
    std::shared_ptr<Message> getNextMessage();

    // Expose lock and condition to derived classes (analogous to protected mLock in Java)
    std::condition_variable mLockCV;
    std::mutex mLockMutex;

private:
    std::deque<std::shared_ptr<Message>> mMessages;
    std::mutex mMessagesMutex;
};

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org