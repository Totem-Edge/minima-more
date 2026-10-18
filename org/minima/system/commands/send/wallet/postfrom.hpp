#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations to minimize header coupling
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace database { namespace userprefs { namespace txndb { class TxnDB; class TxnRow; } } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace system { namespace commands { class CommandRunner; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

class postfrom final : public org::minima::system::commands::Command {
public:
    postfrom();

    // Note: Base Command::getValidParams() is not virtual; this hides it to mirror Java's behavior.
    std::vector<std::string> getValidParams() const;

    // Core execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helper to run a nested command and return its first JSONObject result
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommandInternal(const std::string& zCommand);
};

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org