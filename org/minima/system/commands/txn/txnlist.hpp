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

class txnlist final : public org::minima::system::commands::Command {
public:
    txnlist();

    // Help and params (provided to match Java functionality)
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