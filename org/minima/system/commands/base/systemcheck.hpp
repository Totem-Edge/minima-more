#pragma once

#include <string>
#include <vector>

// Base class include (Inheritance Rule)
#include "org/minima/system/commands/command.hpp"

// Forward declarations to avoid heavy includes (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class MessageProcessor; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class systemcheck final : public org::minima::system::commands::Command {
public:
    systemcheck();

    // Not virtual in base, but we provide same signature to match usage pattern
    std::vector<std::string> getValidParams() const;

    // Main execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory method
    org::minima::system::commands::Command* getFunction() override;

    // Helpers (public as in Java)
    void printDetails(org::minima::utils::messages::MessageProcessor* zProc);
    org::minima::utils::json::JSONObject getInfo(org::minima::utils::messages::MessageProcessor* zProc);
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org