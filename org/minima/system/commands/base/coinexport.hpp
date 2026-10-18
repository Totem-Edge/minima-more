#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class coinexport final : public org::minima::system::commands::Command {
public:
    coinexport();

    // Help text
    std::string getFullHelp() const override;
    // Valid parameters list
    std::vector<std::string> getValidParams() const;

    // Core execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory-style clone
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org