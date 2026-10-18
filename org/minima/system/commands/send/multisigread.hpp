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

class multisigread : public Command {
public:
    // Static contract (Java: public static final String)
    static const std::string MULTISIG_CONTRACT;

    multisigread();

    // Help and params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command API
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    Command* getFunction() override;

private:
    // Helper to resolve a file path (Java returned File)
    std::filesystem::path getRequiredFile();
};

}
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org