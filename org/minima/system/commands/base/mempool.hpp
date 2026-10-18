#pragma once

#include <memory>
#include <string>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class mempool final : public org::minima::system::commands::Command {
public:
    mempool();

    // Overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    // Defaulted special members
    ~mempool() override = default;
    mempool(mempool&&) noexcept = default;
    mempool& operator=(mempool&&) noexcept = default;

    mempool(const mempool&) = delete;
    mempool& operator=(const mempool&) = delete;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org