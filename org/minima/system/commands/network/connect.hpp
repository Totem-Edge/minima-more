#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations for project classes used in signatures (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class Message; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class connect : public org::minima::system::commands::Command {
public:
    connect();

    // Help and params (base methods are not virtual; do not mark override)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory (clone)
    org::minima::system::commands::Command* getFunction() override;

    // Static helper to construct the connect message; returns nullptr on parse error
    static std::shared_ptr<org::minima::utils::messages::Message> createConnectMessage(const std::string& zFullHost);
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org