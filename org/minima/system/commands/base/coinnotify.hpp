#pragma once

#include "org/minima/system/commands/command.hpp"

#include <string>
#include <vector>
#include <memory>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class coinnotify : public org::minima::system::commands::Command {
public:
    coinnotify();

    // Descriptive info (matches base signatures)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

    ~coinnotify() override = default;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org