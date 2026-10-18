#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations for project dependencies used in signatures/bodies
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace system { class Main; } } }
namespace org { namespace minima { namespace system { namespace commands { namespace sendpoll { class SendPollManager; class SendPollMessage; } } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

class sendpoll : public org::minima::system::commands::Command {
public:
    sendpoll();

    // Help and parameter specification
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org