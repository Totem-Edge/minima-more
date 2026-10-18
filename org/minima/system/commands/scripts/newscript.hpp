#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

class newscript final : public org::minima::system::commands::Command {
public:
    newscript();

    // Note: In the provided base header these are not virtual. We match the signatures.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Required overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org