#pragma once

#include <vector>
#include <string>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnminepost : public org::minima::system::commands::Command {
public:
    txnminepost();
    ~txnminepost() override = default;

    // Functions
    std::vector<std::string> getValidParams() const;
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org