#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace signatures {

class sign final : public org::minima::system::commands::Command {
public:
    sign();
    ~sign() override = default;

    // Help and params (base header doesn't mark these virtual; matching signatures provided)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace signatures
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org