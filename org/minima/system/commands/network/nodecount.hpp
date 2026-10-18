#pragma once

#include <vector>
#include <string>
#include <memory>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class nodecount : public org::minima::system::commands::Command {
public:
    nodecount();

    // Hides base non-virtual in provided skeleton; matches Java intent
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

    virtual ~nodecount() = default;
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org