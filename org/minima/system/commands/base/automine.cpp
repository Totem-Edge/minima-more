#include "org/minima/system/commands/base/automine.hpp"

#include "org/minima/system/commands/command.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <string>
#include <utility>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

automine::automine()
    : org::minima::system::commands::Command(
          "automine",
          "[enable:true|false|single] - Simulate traffic") {}

automine::~automine() = default;

std::unique_ptr<org::minima::utils::json::JSONObject> automine::runCommand() {
    auto ret = getJSONReply();

    // Read the parameter with default "", matching Java behavior.
    // No further action is taken as the original Java logic is commented out.
    std::string enable = getParam("enable", "");

    return ret;
}

org::minima::system::commands::Command* automine::getFunction() {
    return new automine();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org