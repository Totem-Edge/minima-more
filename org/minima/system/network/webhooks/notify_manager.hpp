#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>

// Base class include (Inheritance Rule)
#include "org/minima/utils/messages/message_processor.hpp"

// Forward declarations for project classes used in signatures
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class Message; } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace webhooks {

class NotifyManager : public org::minima::utils::messages::MessageProcessor {
public:
    // Post a message to all listeners
    static constexpr const char* NOTIFY_POST = "NOTIFY_POST";

    // Public to match Java's public field
    bool WEBHOOKS_ERROR_LOGS {false};

    NotifyManager();

    void shutDown();

    // Post an event to all the listeners
    void PostEvent(const std::shared_ptr<org::minima::utils::json::JSONObject>& zEvent);

    // Webhook management
    std::vector<std::string> getAllWebHooks();
    void addHook(const std::string& zHook);
    void removeHook(const std::string& zHook);
    void clearHooks();

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    // RPC listeners
    std::vector<std::string> mHooks;
    std::mutex mHooksMutex;
};

} // namespace webhooks
} // namespace network
} // namespace system
} // namespace minima
} // namespace org