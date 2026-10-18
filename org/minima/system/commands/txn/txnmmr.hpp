#pragma once

#include <vector>
#include <string>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnmmr : public org::minima::system::commands::Command {
public:
    txnmmr();

    // Not virtual in base, so do not mark override
    std::vector<std::string> getValidParams() const;

    // Overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org