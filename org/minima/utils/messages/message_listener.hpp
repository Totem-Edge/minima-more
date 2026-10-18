#pragma once

#include <memory>

namespace org {
namespace minima {
namespace utils {
namespace messages {

// Forward declaration of Message to avoid requiring its full definition here.
class Message;

class MessageListener {
public:
    virtual ~MessageListener();

    // Processes the provided message. May throw exceptions on error.
    // Accepts a possibly-null shared_ptr to mirror Java's ability to pass null.
    virtual void processMessage(const std::shared_ptr<Message>& zMessage) = 0;
};

} // namespace messages
} // namespace utils
} // namespace minima
} // namespace org