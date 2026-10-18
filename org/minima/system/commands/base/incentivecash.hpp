#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class incentivecash final : public Command {
public:
    incentivecash();

    // Note: In the provided Command base, these are not virtual, but we provide them
    // to mirror Java functionality.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    Command* getFunction() override;
};

}
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org