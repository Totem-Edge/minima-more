#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnstate final : public org::minima::system::commands::Command {
public:
    txnstate();

    // Not virtual in base, but provided here for functional parity
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Overridden abstract methods
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org