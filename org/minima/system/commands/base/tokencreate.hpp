#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class tokencreate final : public org::minima::system::commands::Command {
public:
    tokencreate();

    // Help text
    std::string getFullHelp() const override;

    // Valid params
    std::vector<std::string> getValidParams() const;

    // Run the command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Return a new instance (like Java getFunction)
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org