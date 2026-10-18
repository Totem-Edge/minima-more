#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace signatures {

class verify final : public org::minima::system::commands::Command {
public:
    verify();

    // Help and params (note: base declaration isn't virtual in provided header)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    ~verify() override = default;
};

} // namespace signatures
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org