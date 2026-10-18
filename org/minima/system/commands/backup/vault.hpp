#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/system/commands/command_exception.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class vault final : public org::minima::system::commands::Command {
public:
    vault();
    ~vault() override = default;

    // Help (base methods are not virtual; do not use override)
    std::string getFullHelp() const override;
    // Valid parameters (base method not virtual; do not use override)
    std::vector<std::string> getValidParams() const;

    // Execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

    // Static helpers (throw CommandException on error)
    static void checkAllKeysCreated();
    static void stopAllKeysCreated();
    static void passwordLockDB(const std::string& zPassword);
    static void passowrdUnlockDB(const std::string& zPassword);
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org