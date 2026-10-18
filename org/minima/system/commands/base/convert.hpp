#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Namespaced forward declarations to avoid heavy includes in header (Pitfall 10)
namespace org { namespace minima { namespace objects { class Address; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniString; } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class convert final : public org::minima::system::commands::Command {
public:
    convert();

    // Note: In provided Command.hpp these are not virtual; we provide same-signature methods.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org