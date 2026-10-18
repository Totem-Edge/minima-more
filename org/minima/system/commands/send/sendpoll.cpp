#include "org/minima/system/commands/send/sendpoll.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "org/minima/system/main.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/sendpoll/send_poll_manager.hpp"
#include "org/minima/system/commands/sendpoll/send_poll_message.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

sendpoll::sendpoll()
    : org::minima::system::commands::Command(
          "sendpoll",
          "(action:add|list|remove) (uid:) - Send function that is added to a list and polls until complete") {}

std::string sendpoll::getFullHelp() const {
    return std::string("\nsendpoll\n"
                       "\n"
                       "Send function that adds 'send' commands to a list and polls every 30 seconds until the return status is 'true'.\n"
                       "\n"
                       "Accepts the same parameters as the 'send' function.\n"
                       "\n"
                       "action: (optional)\n"
                       "    list : list all the 'send' commands in the polling list.\n"
                       "    remove : remove a 'send' command from the polling list.\n"
                       "\n"
                       "uid: (optional)\n"
                       "    The uid of a 'send' command you wish to remove from the polling list. Use with 'action:remove'.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "sendpoll address:0xFF.. amount:10\n"
                       "\n"
                       "sendpoll address:0xFF.. amount:10 tokenid:0xFED5.. burn:0.1\n"
                       "\n"
                       "sendpoll action:list\n"
                       "\n"
                       "sendpoll action:remove uid:0x..\n");
}

std::vector<std::string> sendpoll::getValidParams() const {
    return {
        "action","uid",
        "address","amount","multi","tokenid","state","burn","coinage",
        "split","debug","dryrun","mine","password","storestate",
        "fromaddress","signkey"
    };
}

std::unique_ptr<org::minima::utils::json::JSONObject> sendpoll::runCommand() {
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::json::JSONArray;
    using org::minima::system::commands::CommandException;

    // Base JSON reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Action parameter with default "add"
    const std::string action = getParam("action", "add");

    // Response object to embed in reply
    JSONObject response;

    // Access the SendPollManager
    org::minima::system::commands::sendpoll::SendPollManager* spmgr =
        &org::minima::system::Main::getInstance()->getSendPoll();

    if (action == "add") {
        // The complete command as entered
        std::string command = getCompleteCommand();

        // Replace the first occurrence of "sendpoll" with "send"
        std::string sendcomm = command;
        const std::string from = "sendpoll";
        const std::string to   = "send";
        std::size_t pos = sendcomm.find(from);
        if (pos != std::string::npos) {
            sendcomm.replace(pos, from.size(), to);
        }

        // Ensure it's a valid send command and not just "send"
        if (sendcomm == "send") {
            throw CommandException("Must be a valid send command..");
        }

        // Add to the manager and return the command
        spmgr->addSendCommand(sendcomm);
        response.put("command", sendcomm);

    } else if (action == "list") {
        // Get current list (deep-copied snapshot) and serialize to JSON
        auto commands = spmgr->listCommands();

        JSONArray arr;
        for (const auto& sptr : commands) {
            if (sptr) {
                // toJSON returns a JSONObject by value; store it directly
                arr.add(sptr->toJSON());
            }
        }

        response.put("commands", arr);
        response.put("total", static_cast<std::int64_t>(arr.size()));

    } else if (action == "remove") {
        // Get the uid param and request removal
        const std::string uid = getParam("uid");
        spmgr->removeCommand(uid);
        response.put("removed", uid);
    }

    // Attach response
    ret->put("response", response);

    return ret;
}

org::minima::system::commands::Command* sendpoll::getFunction() {
    return new sendpoll();
}

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org