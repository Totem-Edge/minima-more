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

// Live memory breakdown: process RSS (total/anonymous/file), database file
// sizes (incl. WAL), and the sizes of the in-RAM structures. Read-only.
class memreport : public org::minima::system::commands::Command {
public:
    memreport();

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
