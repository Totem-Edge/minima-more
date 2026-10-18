#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnview final : public org::minima::system::commands::Command {
public:
    txnview();

    // Help text
    std::string getFullHelp() const override;
    // Valid parameters
    std::vector<std::string> getValidParams() const;

    // Core execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory (Java: getFunction)
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org