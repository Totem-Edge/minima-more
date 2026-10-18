#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Namespaced forward declarations to avoid heavy includes in header (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { class MiniFile; } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

class sendview : public org::minima::system::commands::Command {
public:
    sendview();
    ~sendview() override = default;

    // Parameter helpers
    std::vector<std::string> getValidParams() const;
    std::string getFullHelp() const override;
    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org