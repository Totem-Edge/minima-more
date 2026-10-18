#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txninput final : public org::minima::system::commands::Command {
public:
    txninput();

    // Help text
    std::string getFullHelp() const override;
    // Valid params
    std::vector<std::string> getValidParams();

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org