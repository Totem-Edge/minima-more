#pragma once

#include "org/minima/system/commands/command.hpp"

#include <string>
#include <vector>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txncreate final : public org::minima::system::commands::Command {
public:
    txncreate();

    // Help/details
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org