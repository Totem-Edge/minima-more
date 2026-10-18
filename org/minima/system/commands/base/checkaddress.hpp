#pragma once

#include "org/minima/system/commands/command.hpp"
#include <vector>
#include <string>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class checkaddress : public org::minima::system::commands::Command {
public:
    checkaddress();

    // Not virtual in base, so no override keyword here
    std::vector<std::string> getValidParams() const;

    // Overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org