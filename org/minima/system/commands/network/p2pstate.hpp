#pragma once

#include <memory>
#include <string>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class p2pstate final : public org::minima::system::commands::Command {
public:
    p2pstate();

    // Overrides
    std::string getFullHelp() const override;
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org