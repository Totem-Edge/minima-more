#pragma once

#include "org/minima/system/commands/command.hpp"

#include <string>
#include <vector>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class random : public org::minima::system::commands::Command {
public:
    random();

    // Help and params (mirror Java API; not virtual in provided base, but supplied here)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org