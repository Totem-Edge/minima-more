#include "org/minima/utils/messages/message_stack.hpp"

#include <algorithm>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/messages/message.hpp"

namespace org {
namespace minima {
namespace utils {
namespace messages {

MessageStack::MessageStack() = default;

MessageStack::~MessageStack() = default;

void MessageStack::PostMessage(const std::string& zMessage) {
    // Create a new Message and post it
    auto msg = std::make_shared<Message>(zMessage);
    PostMessage(msg);
}

void MessageStack::PostMessage(const std::shared_ptr<Message>& zMessage) {
    {
        std::lock_guard<std::mutex> lock(mMessagesMutex);
        mMessages.push_back(zMessage);
    }
    // There is something in the stack
    notifyLock();
}

void MessageStack::notifyLock() {
    // Wake the thread(s)
    // No need to hold mLockMutex to notify; waiters will use mLockMutex with mLockCV
    try {
        mLockCV.notify_all();
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log("MESSAGE STACK notifyLock exception: " + std::string(exc.what()));
    }
}

bool MessageStack::isNextMessage() {
    std::lock_guard<std::mutex> lock(mMessagesMutex);
    return !mMessages.empty();
}

std::shared_ptr<Message> MessageStack::getNextMessage() {
    std::shared_ptr<Message> nxtmsg;
    {
        std::lock_guard<std::mutex> lock(mMessagesMutex);
        if (!mMessages.empty()) {
            nxtmsg = mMessages.front();
            mMessages.pop_front();
        }
    }
    return nxtmsg;
}

int MessageStack::getSize() {
    std::lock_guard<std::mutex> lock(mMessagesMutex);
    return static_cast<int>(mMessages.size());
}

void MessageStack::clear() {
    std::lock_guard<std::mutex> lock(mMessagesMutex);
    mMessages.clear();
}

void MessageStack::clearExcept(const std::vector<std::string>& zExclude) {
    std::lock_guard<std::mutex> lock(mMessagesMutex);

    std::deque<std::shared_ptr<Message>> newMessages;

    // Create a new list of survivors
    for (const auto& msg : mMessages) {
        const std::string& mtype = msg->getMessageType();
        if (std::find(zExclude.begin(), zExclude.end(), mtype) != zExclude.end()) {
            newMessages.push_back(msg);
        }
    }

    // Clear the old list
    mMessages.clear();

    // Now add the new list
    for (const auto& msg : newMessages) {
        mMessages.push_back(msg);
    }
}

void MessageStack::clearExceptString(const std::string& zExclude) {
    std::lock_guard<std::mutex> lock(mMessagesMutex);

    std::deque<std::shared_ptr<Message>> newMessages;

    // Create a new list of survivors
    for (const auto& msg : mMessages) {
        const std::string& mtype = msg->getMessageType();
        if (mtype.find(zExclude) != std::string::npos) {
            newMessages.push_back(msg);
        }
    }

    // Clear the old list
    mMessages.clear();

    // Now add the new list
    for (const auto& msg : newMessages) {
        mMessages.push_back(msg);
    }
}

void MessageStack::printAllMessages() {
    std::lock_guard<std::mutex> lock(mMessagesMutex);
    int count = 0;
    for (const auto& msg : mMessages) {
        org::minima::utils::MinimaLogger::log(
            std::string("MSG_PROC [") + std::to_string(count) + "] : " + msg->toString(),
            false
        );
        ++count;
    }
}

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org