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

// RFC-0011 §5: expose the generated capability manifest over the command
// surface (so `capabilities` works from the console and RPC as well as MCP).
class capabilities : public org::minima::system::commands::Command {
public:
    capabilities();

    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const override;
    std::vector<ParamSpec> getParamSpecs() const override;
    CommandAnnotations getAnnotations() const override;

    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
