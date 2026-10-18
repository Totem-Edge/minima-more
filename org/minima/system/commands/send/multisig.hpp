#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

class multisig final : public org::minima::system::commands::Command {
public:
    // Static contract string (must never change)
    static const std::string MULTISIG_CONTRACT;

    multisig();

    // Help (base methods are not virtual; do not use override)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execute
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;

private:
    // Utility: convert "file" parameter into a concrete filesystem path
    std::filesystem::path getRequiredFile();
};

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org