#pragma once

#include "org/minima/system/commands/command.hpp"

#include <memory>
#include <string>
#include <vector>

namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class network final : public org::minima::system::commands::Command {
public:
    network();
    ~network() = default;

    // Help and parameter listing
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory method
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org