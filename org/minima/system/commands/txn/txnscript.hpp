#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnscript final : public org::minima::system::commands::Command {
public:
    txnscript();

    // Help/params (non-virtual in base, but provided for parity)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core command API
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org