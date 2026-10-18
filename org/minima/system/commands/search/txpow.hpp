#pragma once

#include "org/minima/system/commands/command.hpp"

#include <memory>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

class txpow final : public org::minima::system::commands::Command {
public:
    txpow();
    ~txpow() override = default;

    // Non-copyable, allow moves
    txpow(const txpow&) = delete;
    txpow& operator=(const txpow&) = delete;
    txpow(txpow&&) noexcept = default;
    txpow& operator=(txpow&&) noexcept = default;

    // Overrides from Command
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org