#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class coinimport final : public org::minima::system::commands::Command {
public:
    coinimport();

    // Informational (non-virtual in base)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Overrides
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    ~coinimport() override = default;
    coinimport(coinimport&&) noexcept = default;
    coinimport& operator=(coinimport&&) noexcept = default;

    coinimport(const coinimport&) = delete;
    coinimport& operator=(const coinimport&) = delete;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org