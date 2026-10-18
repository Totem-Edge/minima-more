#include "org/minima/system/commands/sendpoll/send_poll_manager.hpp"

#include <chrono>
#include <thread>

#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/sendpoll/send_poll_message.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/timer_message.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace sendpoll {

// Static member definitions
const std::string SendPollManager::SENDPOLL_FUNCTION = "SENDPOLL_FUNCTION";
std::int64_t      SendPollManager::SENDPOLL_TIMER    = 1000 * 30;

const std::string SendPollManager::SENDPOLL_LIST   = "SENDPOLL_CLEAR"; // per original Java
const std::string SendPollManager::SENDPOLL_CLEAR  = "SENDPOLL_LIST";  // per original Java

SendPollManager::SendPollManager()
    : org::minima::utils::messages::MessageProcessor("SENDPOLL_MANAGER") {
    // Schedule initial timer
    PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(
        SENDPOLL_TIMER, SENDPOLL_FUNCTION));

    startMessageProcessorThread();
}

// Define destructor out-of-line to handle unique_ptr to incomplete type
SendPollManager::~SendPollManager() = default;

void SendPollManager::addSendCommand(const std::string& zCommand) {
    std::lock_guard<std::mutex> lock(mSyncMutex);
    mSendCommands.emplace_back(std::make_unique<org::minima::system::commands::sendpoll::SendPollMessage>(zCommand));
}

std::vector<std::unique_ptr<org::minima::system::commands::sendpoll::SendPollMessage>>
SendPollManager::listCommands() {
    std::vector<std::unique_ptr<org::minima::system::commands::sendpoll::SendPollMessage>> copy;
    std::lock_guard<std::mutex> lock(mSyncMutex);
    copy.reserve(mSendCommands.size());
    for (const auto& command : mSendCommands) {
        if (command) {
            // Deep copy each entry by reconstructing from fields (mirrors Java copy())
            copy.emplace_back(std::make_unique<org::minima::system::commands::sendpoll::SendPollMessage>(
                command->getUID(), command->getCommand()));
        }
    }
    return copy;
}

void SendPollManager::removeCommand(const std::string& zUID) {
    std::vector<std::unique_ptr<org::minima::system::commands::sendpoll::SendPollMessage>> filtered;
    {
        std::lock_guard<std::mutex> lock(mSyncMutex);
        filtered.reserve(mSendCommands.size());
        for (auto& command : mSendCommands) {
            if (command && command->getUID() != zUID) {
                filtered.emplace_back(std::move(command));
            }
        }
        mSendCommands.clear();
        mSendCommands.swap(filtered);
    }
}

void SendPollManager::processMessage(org::minima::utils::messages::Message& zMessage) {
    if (zMessage.getMessageType() == SENDPOLL_FUNCTION) {
        // Get a copy of the list (deep-copy snapshot)
        auto commands = listCommands();

        for (const auto& commandPtr : commands) {
            if (!isRunning()) {
                break;
            }
            if (!commandPtr) {
                continue;
            }

            try {
                // Execute command
                auto runner = org::minima::system::commands::CommandRunner::getRunner();
                auto res    = runner ? runner->runSingleCommand(commandPtr->getCommand()) : nullptr;

                bool status = false;
                if (res) {
                    try {
                        status = res->getBoolean("status");
                    } catch (const std::exception&) {
                        status = false;
                    }
                }

                if (status) {
                    // Remove from the list on success
                    removeCommand(commandPtr->getUID());
                }
            } catch (const std::exception&) {
                // Mirror Java behavior: ignore and continue
            }

            // Pause 1s between attempts
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        // Schedule next poll
        PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(
            SENDPOLL_TIMER, SENDPOLL_FUNCTION));
    }
}

} // namespace sendpoll
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org