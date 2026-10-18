#include "org/minima/system/commands/network/disconnect.hpp"

#include <stdexcept>

#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

disconnect::disconnect()
    : org::minima::system::commands::Command(
          "disconnect",
          "[uid:uid] - Disconnect from a connected or connecting host") {
}

std::string disconnect::getFullHelp() const {
    return "\ndisconnect\n"
           "\n"
           "Disconnect from a connected or connecting host.\n"
           "\n"
           "Optionally disconnect from all hosts.\n"
           "\n"
           "uid:\n"
           "    Use 'all' to disconnect from all hosts or enter the uid of the host to disconnect from.\n"
           "    uid can be found from the 'network' command.\n"
           "\n"
           "Examples:\n"
           "\n"
           "disconnect uid:CVNPMLPOCQ0HQ\n"
           "\n"
           "disconnect uid:all\n";
}

std::vector<std::string> disconnect::getValidParams() const {
    return std::vector<std::string>{ "uid" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> disconnect::runCommand() {
    // Create the standard JSON reply
    auto ret = getJSONReply();

    // Retrieve uid similar to Java's (String) getParams().get("uid");
    auto& params = getParams();

    std::string uid;
    if (!params.containsKey("uid")) {
        throw std::runtime_error("No uid specified");
    } else {
        uid = params.getString("uid");
    }

    // Perform the disconnect logic
    if (uid == "all") {
        // Post a message to disconnect all (use string overload as in Java semantics)
        org::minima::system::Main::getInstance()->getNIOManager()
            .PostMessage(org::minima::system::network::minima::NIOManager::NIO_DISCONNECTALL);
    } else {
        // Disconnect a specific uid
        org::minima::system::Main::getInstance()->getNIOManager().disconnect(uid);
    }

    // Populate response
    ret->put("status", true);
    ret->put("message", std::string("Attempting to disconnect from ") + uid);

    return ret;
}

org::minima::system::commands::Command* disconnect::getFunction() {
    return new disconnect();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org