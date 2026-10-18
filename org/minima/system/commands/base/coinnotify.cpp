#include "org/minima/system/commands/base/coinnotify.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <string>
#include <vector>
#include <memory>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

coinnotify::coinnotify()
    : org::minima::system::commands::Command(
          "coinnotify",
          "[action:] [address:] - listen for a specific coin address and send a NOTIFYCOIN message when found in chain") {
}

std::string coinnotify::getFullHelp() const {
    return std::string("\ncoinnotify\n")
        + "\n"
        + "Listen for a specific coin address - without adding it to scripts.\n"
        + "You need to do this every startup.. from your Minidapp service.js for example\n"
        + "\n"
        + "action: \n"
        + "    add : Add to the list - only added once if already added.\n"
        + "    remove : Remove from the list\n"
        + "    check : Check if in the list.\n"
        + "\n"
        + "address:\n"
        + "    The address to look out for.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "coinnotify action:add address:0xFFEEDD..\n"
        + "\n"
        + "coinnotify action:remove address:0xFFEEDD..\n"
        + "\n"
        + "coinnotify action:check address:Mx12ABGF56..\n";
}

std::vector<std::string> coinnotify::getValidParams() const {
    return std::vector<std::string>{ "action", "address" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> coinnotify::runCommand() {
    using org::minima::utils::json::JSONObject;
    using org::minima::database::MinimaDB;

    // Base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Which action (throws if missing/blank)
    std::string action = getParam("action");

    // Get the normalized address (throws if invalid/missing)
    std::string addr = getAddressParam("address");

    // Build response
    JSONObject resp;
    resp.put("address", addr);

    if (action == "add") {
        MinimaDB::getDB()->addCoinNotify(addr);

    } else if (action == "remove") {
        bool found = MinimaDB::getDB()->removeCoinNotify(addr);
        resp.put("found", found);

    } else if (action == "check") {
        bool found = MinimaDB::getDB()->checkCoinNotify(addr);
        resp.put("found", found);

    } else {
        throw org::minima::system::commands::CommandException("Invalid action : " + action);
    }

    // Attach response
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* coinnotify::getFunction() {
    return new coinnotify();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org