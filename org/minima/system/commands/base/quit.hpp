#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class quit : public org::minima::system::commands::Command {
public:
    quit();

    // Note: getFullHelp and getValidParams are not virtual in the provided base class.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Virtual overrides from Command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org