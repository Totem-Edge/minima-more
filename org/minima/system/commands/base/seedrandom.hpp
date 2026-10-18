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

class seedrandom final : public org::minima::system::commands::Command {
public:
    seedrandom();

    // Help text
    std::string getFullHelp() const override;
    // Valid parameters
    std::vector<std::string> getValidParams() const;

    // Command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

    // Rule of 5
    ~seedrandom() override = default;
    seedrandom(seedrandom&&) noexcept = default;
    seedrandom& operator=(seedrandom&&) noexcept = default;

    seedrandom(const seedrandom&) = delete;
    seedrandom& operator=(const seedrandom&) = delete;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org