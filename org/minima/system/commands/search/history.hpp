#pragma once

#include "org/minima/system/commands/command.hpp"

#include <memory>
#include <string>
#include <vector>

namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

class history final : public org::minima::system::commands::Command {
public:
    history();

    // Help
    std::string getFullHelp() const override;
    // Valid params
    std::vector<std::string> getValidParams() const;

    // Execute command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

private:
    // Build per-transaction details identical to Java logic
    org::minima::utils::json::JSONObject getTxnDetails(const org::minima::objects::TxPoW& zTxPoW);
};

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org