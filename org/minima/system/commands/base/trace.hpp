#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class trace : public org::minima::system::commands::Command {
public:
    trace();

    // Help
    std::string getFullHelp() const override;
    // Valid parameters
    std::vector<std::string> getValidParams() const;

    // Execute command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org