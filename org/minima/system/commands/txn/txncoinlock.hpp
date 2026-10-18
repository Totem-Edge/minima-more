#pragma once

#include "org/minima/system/commands/command.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declaration for project JSON type (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txncoinlock : public org::minima::system::commands::Command {
public:
    txncoinlock();
    virtual ~txncoinlock();

    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org