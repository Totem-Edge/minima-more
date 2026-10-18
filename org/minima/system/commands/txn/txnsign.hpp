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

class txnsign final : public org::minima::system::commands::Command {
public:
    txnsign();

    // Help text (base class method is not virtual in provided headers)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org