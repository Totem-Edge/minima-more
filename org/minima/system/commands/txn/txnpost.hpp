#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

// Forward declarations to avoid heavy includes in header (Pitfall 10)
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { class Transaction; } } }
namespace org { namespace minima { namespace objects { class Witness; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnpost final : public org::minima::system::commands::Command {
public:
    txnpost();

    // Help/info
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    // Also used by TxnSign if autopost set (Java static method)
    static std::unique_ptr<org::minima::objects::TxPoW>postTxn(const std::string& zID,
                                               const org::minima::objects::base::MiniNumber& zBurn,
                                               bool zAuto,
                                               bool zMineSync);
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org