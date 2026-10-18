#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

class sendpost final : public org::minima::system::commands::Command {
public:
    sendpost();

    // Param/Help
    std::vector<std::string> getValidParams() const;
    std::string getFullHelp() const override;
    // Execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org