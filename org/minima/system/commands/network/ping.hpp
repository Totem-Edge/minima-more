#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class ping : public org::minima::system::commands::Command {
public:
    ping();

    ~ping() override = default;

    // Overrides
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org