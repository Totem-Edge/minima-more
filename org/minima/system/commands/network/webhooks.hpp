#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations for JSON types used in signatures (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace system { class Main; } } }
namespace org { namespace minima { namespace system { namespace network { namespace webhooks { class NotifyManager; } } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class webhooks : public org::minima::system::commands::Command {
public:
    webhooks();

    // Help and valid params (non-virtual in provided base; implemented to match Java behavior)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org