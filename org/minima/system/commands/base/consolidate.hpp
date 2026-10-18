#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class consolidate : public org::minima::system::commands::Command {
public:
    consolidate();
    ~consolidate() override = default;

    // Help / params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org