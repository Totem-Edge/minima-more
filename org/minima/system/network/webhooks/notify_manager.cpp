#include "org/minima/system/network/webhooks/notify_manager.hpp"

#include <algorithm>

#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/message_listener.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/r_p_c_client.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/system/main.hpp"

// Full header for UserDB to access getWebHooks/setWebHooks
#include "org/minima/database/userprefs/user_d_b.hpp"

// Windows headers (pulled via OpenSSL) define a PostMessage macro that conflicts with our method.
// Undefine it to ensure calls resolve to the class member function.
#ifdef PostMessage
#undef PostMessage
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace webhooks {

using org::minima::utils::messages::Message;
using org::minima::utils::messages::MessageListener;
using org::minima::utils::json::JSONObject;
using org::minima::utils::MinimaLogger;
using org::minima::utils::RPCClient;
using org::minima::database::MinimaDB;
using org::minima::system::Main;

NotifyManager::NotifyManager()
    : org::minima::utils::messages::MessageProcessor("NOTIFYMANAGER") {
    //
    // FIX 1: Get UserDB as a reference (auto&)
    //
    auto& udb = MinimaDB::getDB()->getUserDB();
    
    //
    // FIX 2: Remove invalid null check and use dot '.' operator
    //
    std::lock_guard<std::mutex> lock(mHooksMutex);
    mHooks = udb.getWebHooks();

    // Start worker thread only after all derived-class members are initialized.
    startMessageProcessorThread();
}

void NotifyManager::shutDown() {
    // Stop this processor
    stopMessageProcessor();
}

void NotifyManager::PostEvent(const std::shared_ptr<JSONObject>& zEvent) {
    Message msg(NOTIFY_POST);
    msg.addObject("notify", zEvent);
    // Post to the processor queue
    std::shared_ptr<Message> sp = std::make_shared<Message>(msg);
    PostMessage(sp);
}

std::vector<std::string> NotifyManager::getAllWebHooks() {
    std::vector<std::string> ret;
    std::lock_guard<std::mutex> lock(mHooksMutex);
    ret.reserve(mHooks.size());
    for (const auto& hook : mHooks) {
        ret.emplace_back(hook);
    }
    return ret;
}

void NotifyManager::addHook(const std::string& zHook) {
    std::lock_guard<std::mutex> lock(mHooksMutex);
    if (!zHook.empty() &&
        std::find(mHooks.begin(), mHooks.end(), zHook) == mHooks.end()) {
        mHooks.emplace_back(zHook);
    }

    //
    // FIX 1: Get UserDB as a reference (auto&)
    //
    auto& udb = MinimaDB::getDB()->getUserDB();
    
    //
    // FIX 2: Remove invalid null check and use dot '.' operator
    //
    udb.setWebHooks(mHooks);
}

void NotifyManager::removeHook(const std::string& zHook) {
    std::lock_guard<std::mutex> lock(mHooksMutex);

    mHooks.erase(std::remove(mHooks.begin(), mHooks.end(), zHook), mHooks.end());

    //
    // FIX 1: Get UserDB as a reference (auto&)
    //
    auto& udb = MinimaDB::getDB()->getUserDB();
    
    //
    // FIX 2: Remove invalid null check and use dot '.' operator
    //
    udb.setWebHooks(mHooks);
}

void NotifyManager::clearHooks() {
    std::lock_guard<std::mutex> lock(mHooksMutex);

    mHooks.clear();

    //
    // FIX 1: Get UserDB as a reference (auto&)
    //
    auto& udb = MinimaDB::getDB()->getUserDB();
    
    //
    // FIX 2: Remove invalid null check and use dot '.' operator
    //
    udb.setWebHooks(mHooks);
}

void NotifyManager::processMessage(Message& zMessage) {
    if (zMessage.isMessageType(NOTIFY_POST)) {
// ... (rest of the file is unchanged) ...
        // Get the Message
        std::shared_ptr<JSONObject> notify;
        try {
            // Prefer shared_ptr<JSONObject>
            notify = std::any_cast<std::shared_ptr<JSONObject>>(zMessage.getObject("notify"));
        } catch (const std::bad_any_cast&) {
            // Fallback if stored as value
            try {
                JSONObject nobj = std::any_cast<JSONObject>(zMessage.getObject("notify"));
                notify = std::make_shared<JSONObject>(nobj);
            } catch (const std::bad_any_cast&) {
                // Invalid payload; nothing to do
                return;
            }
        }

        std::string event = notify ? notify->getString("event") : std::string();

        // Is someone listening directly
        MessageListener* minilistener = Main::getMinimaListener();
        if (minilistener != nullptr) {
            try {
                // Create a shared_ptr<Message> matching listener signature
                std::shared_ptr<Message> msgptr = std::make_shared<Message>(zMessage);
                minilistener->processMessage(msgptr);
            } catch (const std::exception& exc) {
                MinimaLogger::log(std::string(exc.what()) + " : " + zMessage.toString());
            } catch (...) {
                MinimaLogger::log(std::string("Unknown exception while notifying listener: ") + zMessage.toString());
            }
        }

        // Convert..
        std::string postmsg = notify ? notify->toString() : std::string();

        // Cycle through and Post to each hook..
        std::vector<std::string> hooks = getAllWebHooks();
        for (const std::string& hook : hooks) {
            std::size_t index = hook.find('#');
            if (index == std::string::npos) {
                continue;
            }

            std::string filter  = hook.substr(0, index);
            std::string webhook = hook.substr(index + 1);

            // Check running..
            if (!isRunning()) {
                return;
            }

            try {
                if (filter.empty() || (!event.empty() && event.find(filter) != std::string::npos)) {
                    // Post it..
                    RPCClient::sendPOST(webhook, postmsg);
                }
            } catch (const std::exception& exc) {
                if (WEBHOOKS_ERROR_LOGS) {
                    MinimaLogger::log(std::string("ERROR webhook : ") + hook + " " + exc.what(), false);
                }
            } catch (...) {
                if (WEBHOOKS_ERROR_LOGS) {
                    MinimaLogger::log(std::string("ERROR webhook : ") + hook + " unknown exception", false);
                }
            }
        }
    }
}

} // namespace webhooks
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
