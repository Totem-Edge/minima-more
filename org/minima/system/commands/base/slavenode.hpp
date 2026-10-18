#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class slavenode : public org::minima::system::commands::Command {
public:
    slavenode();

    // Note: Base class methods are not marked virtual in provided header.
    // We provide same signatures to mirror Java overrides.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Required abstract implementations
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org