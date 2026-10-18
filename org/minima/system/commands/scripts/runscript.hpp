#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

class runscript final : public org::minima::system::commands::Command {
public:
    runscript();

    // Help and params (const signatures to align with base interface style)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execute command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org