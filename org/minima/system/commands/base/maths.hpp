#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations to avoid heavy includes in header (Pitfall 4)
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace objects { class Transaction; class Witness; class StateVariable; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class maths : public org::minima::system::commands::Command {
public:
    maths();

    // Not marked override because base declaration may not be virtual in provided headers
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Required overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org