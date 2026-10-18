#pragma once

#include "org/minima/system/commands/command.hpp"
#include <memory>
#include <string>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class automine : public org::minima::system::commands::Command {
public:
    automine();
    ~automine() override;

    // Delete copy operations
    automine(const automine&) = delete;
    automine& operator=(const automine&) = delete;

    // Allow moves
    automine(automine&&) noexcept = default;
    automine& operator=(automine&&) noexcept = default;

    // Command overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org