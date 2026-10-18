#pragma once

#include <memory>
#include <string>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {

class whitepaper : public Command {
public:
    // Whitepaper content as a static C-string literal.
    static const char* WP;

    // Constructor: name and help passed to base.
    whitepaper();

    // Execute the command: returns a JSON object containing the whitepaper.
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory method: return a new instance (ownership to caller as per base API).
    Command* getFunction() override;
};

} // namespace commands
} // namespace system
} // namespace minima
} // namespace org