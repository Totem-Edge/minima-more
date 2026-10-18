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

class status final : public org::minima::system::commands::Command {
public:
    status();

    // Informational helpers
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    // Special members
    virtual ~status() = default;
    status(status&&) noexcept = default;
    status& operator=(status&&) noexcept = default;
    status(const status&) = delete;
    status& operator=(const status&) = delete;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org