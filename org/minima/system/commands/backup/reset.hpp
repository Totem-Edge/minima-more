#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp" // Base class include (Inheritance Rule)

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

class reset : public org::minima::system::commands::Command {
public:
    reset();

    // Note: In the provided Command.hpp these are not virtual, but we provide them to preserve content.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Abstract overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org