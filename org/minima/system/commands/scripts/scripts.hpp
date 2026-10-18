#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

class scripts final : public org::minima::system::commands::Command {
public:
    scripts();

    // Note: In provided Command.hpp these are not virtual; we still provide them to mirror Java.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    ~scripts() override = default;
    scripts(scripts&&) noexcept = default;
    scripts& operator=(scripts&&) noexcept = default;

    scripts(const scripts&) = delete;
    scripts& operator=(const scripts&) = delete;
};

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org