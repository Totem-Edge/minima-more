#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations are already present in Command.hpp for JSONObject/JSONArray.

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

class signfrom : public org::minima::system::commands::Command {
public:
    signfrom();

    // Valid params list (note: base method is not virtual, so no override specifier)
    std::vector<std::string> getValidParams() const;

    // Main command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helper to run a single command string and return the first JSONObject result
    std::shared_ptr<org::minima::utils::json::JSONObject> runCommandStr(const std::string& zCommand);
};

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org