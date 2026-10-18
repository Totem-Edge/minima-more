#pragma once

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

#include "org/minima/utils/messages/message_processor.hpp"

// Forward declarations for project classes used in members/signatures
namespace org { namespace minima { namespace system { namespace commands { namespace sendpoll { class SendPollMessage; } } } } }
namespace org { namespace minima { namespace utils { namespace messages { class Message; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace sendpoll {

class SendPollManager : public org::minima::utils::messages::MessageProcessor {
public:
    // Static constants mirroring Java
    static const std::string SENDPOLL_FUNCTION;
    static std::int64_t      SENDPOLL_TIMER;

    static const std::string SENDPOLL_LIST;   // Intentionally swapped per original Java source
    static const std::string SENDPOLL_CLEAR;  // Intentionally swapped per original Java source

    SendPollManager();
    virtual ~SendPollManager() override;

    // Non-copyable and non-movable (base is non-movable)
    SendPollManager(const SendPollManager&) = delete;
    SendPollManager& operator=(const SendPollManager&) = delete;
    SendPollManager(SendPollManager&&) = delete;
    SendPollManager& operator=(SendPollManager&&) = delete;

    // API methods
    void addSendCommand(const std::string& zCommand);

    // Returns a deep-copied snapshot of the current commands (each entry is a separate instance)
    std::vector<std::unique_ptr<org::minima::system::commands::sendpoll::SendPollMessage>> listCommands();

    void removeCommand(const std::string& zUID);

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    // Storage for pending send commands
    std::vector<std::unique_ptr<org::minima::system::commands::sendpoll::SendPollMessage>> mSendCommands;

    // Synchronization primitive replacing Java's synchronized(mSyncObject)
    std::mutex mSyncMutex;
};

} // namespace sendpoll
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org