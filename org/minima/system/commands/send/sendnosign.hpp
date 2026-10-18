#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

class sendnosign final : public org::minima::system::commands::Command {
public:
    sendnosign();
    ~sendnosign() override;

    std::vector<std::string> getValidParams() const;
    std::string getFullHelp() const override;
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

private:
    // Pass-through coin selection: preserves order (Java's send.selectCoins is not available here).
    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    selectCoins(const std::vector<std::shared_ptr<org::minima::objects::Coin>>& coins,
                const org::minima::objects::base::MiniNumber& target,
                bool debug);
};

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org