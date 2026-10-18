#pragma once

#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"

// Forward declaration to keep header light
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

class coincheck : public org::minima::system::commands::Command {
public:
    coincheck();

    // Help
    std::string getFullHelp() const override;
    // Valid params
    std::vector<std::string> getValidParams() const;

    // Main execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org