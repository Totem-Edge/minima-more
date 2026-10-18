#pragma once

#include "org/minima/system/commands/command.hpp"

#include <string>
#include <vector>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class debugflag : public org::minima::system::commands::Command {
public:
    debugflag();

    // Note: Base getValidParams() in the provided skeleton is not virtual.
    // We provide this method to mirror Java behavior.
    std::vector<std::string> getValidParams() const;

    // Overrides from Command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org