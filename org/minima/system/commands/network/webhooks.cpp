#include "org/minima/system/commands/network/webhooks.hpp"

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/main.hpp"
#include "org/minima/system/network/webhooks/notify_manager.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

webhooks::webhooks()
    : org::minima::system::commands::Command(
          "webhooks",
          "(action:list|add|remove|clear) (hook:url) - Add a web hook that is called with Minima events as they happen") {
}

std::string webhooks::getFullHelp() const {
    return "\nwebhooks\n"
           "\n"
           "Add a web hook that is called with Minima events as they happen.\n"
           "\n"
           "POST requests, so the URL must be a POST endpoint.\n"
           "\n"
           "action: (optional)\n"
           "    list : List your existing webhooks. The default.\n"
           "    add : Add a new webhook. \n"
           "    remove : Remove an existing webhook.\n"
           "    clear : Clear the existing webhooks.\n"
           "\n"
           "hook: (optional)\n"
           "    A URL, must be a POST endpoint.\n"
           "\n"
           "filter: (optional)\n"
           "    Filters which events get posted.\n"
           "\n"
           "Examples:\n"
           "\n"
           "webhooks action:list\n"
           "\n"
           "webhooks action:add hook:http://127.0.0.1/myapi.php\n"
           "\n"
           "webhooks action:remove hook:http://127.0.0.1/myapi.php\n"
           "\n"
           "webhooks action:add hook:http://127.0.0.1/myapi.php filter:MINING\n"
           "\n"
           "webhooks action:clear\n";
}

std::vector<std::string> webhooks::getValidParams() const {
    return std::vector<std::string>{"enable", "action", "hook", "filter"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> webhooks::runCommand() {
    // Create the base reply object
    auto ret = getJSONReply();

    // Get the action, default to "list"
    std::string action = getParam("action", "list");

    // Get the NotifyManager
    org::minima::system::network::webhooks::NotifyManager* notify =
        &org::minima::system::Main::getInstance()->getNotifyManager();

    // Build the response object (populate fully before inserting into ret)
    org::minima::utils::json::JSONObject resp;

    if (action == "add") {
        std::string filter = getParam("filter", "");
        std::string hook = getParam("hook");

        std::string fullhook = filter + "#" + hook;
        notify->addHook(fullhook);

    } else if (action == "remove") {
        std::string hook = getParam("hook");
        notify->removeHook(hook);

    } else if (action == "clear") {
        notify->clearHooks();

    } else if (action == "errorlogs") {
        bool enable = getBooleanParam("enable");
        org::minima::system::Main::getInstance()->getNotifyManager().WEBHOOKS_ERROR_LOGS = enable;
    }

    // List all the current hooks
    std::vector<std::string> hooks = notify->getAllWebHooks();
    org::minima::utils::json::JSONArray arr;
    for (const std::string& hook : hooks) {
        arr.add(hook);
    }

    // Put results into response
    resp.put("webhooks", arr);

    // Attach response to ret
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* webhooks::getFunction() {
    return new webhooks();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org