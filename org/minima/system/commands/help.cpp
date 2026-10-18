#include "org/minima/system/commands/help.hpp"

#include <algorithm>
#include <memory>
#include <vector>
#include <string>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org { namespace minima { namespace system { namespace commands {

help::help()
    : org::minima::system::commands::Command(
          "help",
          "Show Help. [] are required. () are optional. Use 'help command:' for full help. Chain multiple commands with ;") {
}

std::vector<std::string> help::getValidParams() const {
    return std::vector<std::string>{ "command" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> help::runCommand() {
    using org::minima::utils::json::JSONObject;

    // Base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Details object to populate
    JSONObject details;

    // Use the canonical command registry so help always matches the commands
    // that CommandRunner actually accepts. This avoids drift between the
    // registered command list and the help output.
    auto& protos = CommandRunner::getCommandPrototypes();

    // Parameter
    std::string command = getParam("command", "");

    if (!command.empty()) {
        // Look for a matching command by name
        Command* found = nullptr;
        for (auto& c : protos) {
            if (c && c->getName() == command) {
                found = c.get();
                break;
            }
        }

        if (!found) {
            throw org::minima::system::commands::CommandException("Command not found : " + command);
        }

        details.put("command", command);
        details.put("help", found->getHelp());
        details.put("fullhelp", found->getFullHelp());
    } else {
        // Default command listing - iterate the real registry
        for (const auto& c : protos) {
            if (c) {
                addCommand(details, *c);
            }
        }
    }

    // Attach details
    ret->put("response", details);

    return ret;
}

org::minima::system::commands::Command* help::getFunction() {
    return new help();
}

void help::addCommand(org::minima::utils::json::JSONObject& zDetails,
                      const org::minima::system::commands::Command& zCommand) {
    const std::string key = getStrOfLength(15, zCommand.getName());
    zDetails.put(key, zCommand.getHelp());
}

std::string help::getStrOfLength(int zDesiredLen, const std::string& zString) const {
    std::string ret = zString;
    const int len = static_cast<int>(ret.size());

    if (len >= zDesiredLen) {
        return ret.substr(0, static_cast<std::size_t>(zDesiredLen));
    }

    // Right-pad with spaces
    ret.reserve(static_cast<std::size_t>(zDesiredLen));
    for (int i = 0; i < zDesiredLen - len; ++i) {
        ret.push_back(' ');
    }
    return ret;
}

} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
