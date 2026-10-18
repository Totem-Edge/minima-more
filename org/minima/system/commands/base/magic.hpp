#pragma once

#include <vector>
#include <string>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class magic final : public org::minima::system::commands::Command {
public:
    magic();

    // Not virtual in base, but we provide same signature for parity
    std::vector<std::string> getValidParams() const;

    // Core command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Return a new instance (mirrors Java getFunction())
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org