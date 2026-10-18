#pragma once

#include <memory>
#include <vector>
#include <string>

#include "org/minima/system/commands/command.hpp"

// Forward declarations per Pitfall 10
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class scanchain final : public org::minima::system::commands::Command {
public:
    scanchain();

    // Override
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

private:
    // Returns a new JSONObject with the transaction details (never null)
    std::unique_ptr<org::minima::utils::json::JSONObject>
    getTransactionDetails(const org::minima::objects::TxPoW& zTxPoW) const;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org