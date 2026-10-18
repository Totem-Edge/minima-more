#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

class tokens final : public org::minima::system::commands::Command {
public:
    tokens();

    // These do not use 'override' because the base declarations may not be virtual in this codebase
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Virtual overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org