#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class logs : public org::minima::system::commands::Command {
public:
    logs();

    // Help text (not marked override because base declaration isn't virtual in provided header)
    std::string getFullHelp() const override;
    // Valid params list (not marked override for same reason)
    std::vector<std::string> getValidParams() const;

    // Command overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org