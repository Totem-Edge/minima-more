#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations (Pitfall 10)
namespace org { namespace minima { namespace database { namespace userprefs { namespace txndb { class TxnRow; } } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnauto : public org::minima::system::commands::Command {
public:
    txnauto();

    // Valid params list
    std::vector<std::string> getValidParams() const;

    // Execute the command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Prototype factory
    org::minima::system::commands::Command* getFunction() override;

    // Static helper function (translated from Java)
    static std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>
    createTransaction(const std::string& zAddress,
                      const org::minima::objects::base::MiniNumber& zAmount);
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org