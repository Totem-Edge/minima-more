#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations to keep the header lightweight (Pitfall 4)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {

class CommandRunner {
public:
    // Factory (mirrors Java static getRunner())
    static std::unique_ptr<CommandRunner> getRunner();

    // No copying
    CommandRunner(const CommandRunner&) = delete;
    CommandRunner& operator=(const CommandRunner&) = delete;

    // Execution helpers (mirror Java)
    std::unique_ptr<org::minima::utils::json::JSONArray> runMultiCommand(const std::string& zCommand);
    std::unique_ptr<org::minima::utils::json::JSONObject> runSingleCommand(const std::string& zCommand);
    std::unique_ptr<org::minima::utils::json::JSONArray> runMultiCommand(const std::string& zMiniDAPPID,
                                                                          const std::string& zCommand);

    // Command resolution (mirror Java)
    std::unique_ptr<org::minima::system::commands::Command> getCommandOnly(const std::string& zCommandName);
    std::unique_ptr<org::minima::system::commands::Command> getCommand(const std::string& zCommand);

    // Permission check
    bool isCommandAllowed(const std::string& zCommand);

private:
    CommandRunner();

    // Helpers
    std::vector<std::string> splitStringJSON(bool zForceNormal, const std::string& zInput);
    std::vector<std::string> splitterQuotedPattern(const std::string& zInput);

    // Access prototypes of all known commands (empty by default here; can be extended elsewhere)
    static std::vector<std::unique_ptr<org::minima::system::commands::Command>>& getPrototypes();

public:
    // Public access to the prototype list so help (and other consumers) can enumerate
    // every registered command without maintaining a separate, drifting list.
    static std::vector<std::unique_ptr<org::minima::system::commands::Command>>& getCommandPrototypes() { return getPrototypes(); }

    // Utils
    static std::string trim(const std::string& s);
    static std::string toLower(const std::string& s);
};

} // namespace commands
} // namespace system
} // namespace minima
} // namespace org