#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

// Read-only consistency check across archive, cascade, TxPoWTree and MegaMMR.
class state final : public org::minima::system::commands::Command {
public:
    state();

    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    virtual ~state() = default;
    state(state&&) noexcept = default;
    state& operator=(state&&) noexcept = default;
    state(const state&) = delete;
    state& operator=(const state&) = delete;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
