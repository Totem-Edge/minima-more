#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

class coins : public org::minima::system::commands::Command {
public:
    coins();
    virtual ~coins() = default;
    coins(coins&&) noexcept = default;
    coins& operator=(coins&&) noexcept = default;
    coins(const coins&) = delete;
    coins& operator=(const coins&) = delete;

    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams();
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org