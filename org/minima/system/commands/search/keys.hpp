#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

class keys : public org::minima::system::commands::Command {
public:
    keys();

    // These are not virtual in the base, but we provide them to mirror Java API.
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    // Static utility
    static bool checkKey(const std::string& zPublicKey);
};

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org