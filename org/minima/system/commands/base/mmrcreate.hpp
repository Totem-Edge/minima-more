#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class mmrcreate : public org::minima::system::commands::Command {
public:
    mmrcreate();

    // Help text identical to Java
    std::string getFullHelp() const override;
    // Valid parameters
    std::vector<std::string> getValidParams() const;

    // Command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org