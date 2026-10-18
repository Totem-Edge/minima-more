#pragma once

#include "org/minima/system/commands/command.hpp"

#include <memory>
#include <string>
#include <vector>

// Forward declarations (namespaced)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace objects { class Coin; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

class consolidatefrom : public org::minima::system::commands::Command {
public:
    consolidatefrom();

    // Note: In provided Command.hpp, getValidParams() is not virtual.
    std::vector<std::string> getValidParams() const;

    // Main command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Helper to execute a single embedded command and get its JSON result
    std::shared_ptr<org::minima::utils::json::JSONObject> runCommand(const std::string& zCommand);

    // Create consolidate transaction from a given coin list (posts immediately)
    std::shared_ptr<org::minima::utils::json::JSONObject> createConsolidate(
        const std::vector<std::shared_ptr<org::minima::objects::Coin>>& zAllCoins,
        const org::minima::objects::base::MiniNumber& zBurn,
        const std::string& zFromAddress,
        const std::string& zTokenid,
        const std::string& zScript,
        const std::string& zPrivateKey,
        const org::minima::objects::base::MiniNumber& zKeyUses);

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org