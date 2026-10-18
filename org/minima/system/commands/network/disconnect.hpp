#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class disconnect : public org::minima::system::commands::Command {
public:
    disconnect();

    // Help and params (base does not declare these virtual in provided header)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org