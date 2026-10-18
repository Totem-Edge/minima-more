#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnbasics final : public org::minima::system::commands::Command {
public:
    txnbasics();

    // Help text (hides base non-virtual)
    std::string getFullHelp() const override;
    // Valid parameter list (hides base non-virtual)
    std::vector<std::string> getValidParams() const;

    // Core command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory (as in Java getFunction)
    org::minima::system::commands::Command* getFunction() override;

    ~txnbasics() override = default;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org