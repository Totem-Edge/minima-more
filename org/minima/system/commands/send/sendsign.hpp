#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

class sendsign : public org::minima::system::commands::Command {
public:
    sendsign();
    ~sendsign() override = default;

    // Note: These shadow the base class methods (base does not declare them virtual)
    std::vector<std::string> getValidParams();
    std::string getFullHelp() const override;

    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org