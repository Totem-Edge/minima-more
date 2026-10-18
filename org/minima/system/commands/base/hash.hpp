#pragma once

#include "org/minima/system/commands/command.hpp"

#include <string>
#include <vector>

namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniString; } } } }
namespace org { namespace minima { namespace utils { class Crypto; } } }
namespace org { namespace minima { namespace utils { class MiniFile; } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class hash : public org::minima::system::commands::Command {
public:
    hash();

    // Not marked override since base declarations may not be virtual in provided skeleton
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