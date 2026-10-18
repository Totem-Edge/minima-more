#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class mysqlcoins final : public org::minima::system::commands::Command {
public:
    mysqlcoins();
    ~mysqlcoins() override = default;

    // Help text
    std::string getFullHelp() const override;
    // Valid parameters
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org