#pragma once

#include <memory>
#include <string>
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// ADD THIS: Include the full base class definition
#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {

// REMOVE THIS: Forward declaration no longer needed
// class Command;

// CHANGE THIS: Ensure public inheritance
class tutorial : public Command { // <-- Add 'public'
public:
    tutorial();
    virtual ~tutorial() = default;

    // Execute the command and return a JSON response
    virtual std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Return a new instance of this command
    virtual Command* getFunction() override;
};

} // namespace commands
} // namespace system
} // namespace minima
} // namespace org