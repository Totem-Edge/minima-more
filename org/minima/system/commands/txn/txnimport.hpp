#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations to avoid heavy includes in header (Pitfall 4)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnimport final : public org::minima::system::commands::Command {
public:
    txnimport();

    // Help and params (not marked override as base may not be virtual)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org