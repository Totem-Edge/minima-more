#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

class removescript final : public org::minima::system::commands::Command {
public:
    removescript();
    ~removescript() override = default;

    // Help and params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org