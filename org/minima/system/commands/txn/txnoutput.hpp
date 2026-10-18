#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnoutput final : public org::minima::system::commands::Command {
public:
    txnoutput();

    // Help/params (base methods are non-virtual; do not mark override)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org