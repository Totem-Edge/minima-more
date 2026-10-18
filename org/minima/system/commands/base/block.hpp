#pragma once

#include <memory>
#include <string>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class block final : public org::minima::system::commands::Command {
public:
    block();

    // Informational help (base does not declare this virtual)
    std::string getFullHelp() const override;
    // Inherited virtuals
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    // Destructor
    ~block() override = default;

    // Move operations
    block(block&&) noexcept = default;
    block& operator=(block&&) noexcept = default;

    // Delete copy
    block(const block&) = delete;
    block& operator=(const block&) = delete;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org