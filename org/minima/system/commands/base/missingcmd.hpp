#pragma once

#include "org/minima/system/commands/command.hpp"
#include <memory>
#include <string>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class missingcmd : public org::minima::system::commands::Command {
public:
    // Constructor
    missingcmd(const std::string& zInput, const std::string& zError);

    // Destructor
    ~missingcmd() override = default;

    // Overridden virtuals
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

private:
    std::string mInput;
    std::string mError;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org