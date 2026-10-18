#pragma once

#include <memory>
#include <string>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class tutorial : public org::minima::system::commands::Command {
public:
    tutorial();
    ~tutorial() override = default;

    // Provide full help text (note: base declaration is non-virtual in provided header)
    std::string getFullHelp() const override;
    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org