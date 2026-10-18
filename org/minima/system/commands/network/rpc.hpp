#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class rpc : public org::minima::system::commands::Command {
public:
    rpc();

    // Note: In the provided Command.hpp, these are not virtual; we still provide matching signatures.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Required overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org