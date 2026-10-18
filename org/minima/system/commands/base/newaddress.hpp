#pragma once

#include <memory>
#include <string>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class newaddress final : public org::minima::system::commands::Command {
public:
    newaddress();

    // Note: Base getFullHelp() may not be virtual; we provide the same signature.
    std::string getFullHelp() const override;
    // Required overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org