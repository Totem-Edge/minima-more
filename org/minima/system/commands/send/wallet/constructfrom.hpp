#pragma once

#include "org/minima/system/commands/command.hpp"

#include <memory>
#include <string>
#include <vector>

// Forward declarations for JSON types (already in base, but safe here)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; class MiniData; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

class constructfrom : public org::minima::system::commands::Command {
public:
    constructfrom();
    ~constructfrom() override = default;

    // Not virtual in base, but provided to mirror Java override
    std::vector<std::string> getValidParams() const;

    // Core command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Helper to mirror Java: run a single command string and get the first result JSONObject
    std::shared_ptr<org::minima::utils::json::JSONObject> runCommand(const std::string& zCommand);

    org::minima::system::commands::Command* getFunction() override;
};

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org