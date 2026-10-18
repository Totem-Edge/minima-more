#include "org/minima/system/commands/base/quit.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

quit::quit()
    : org::minima::system::commands::Command(
          "quit",
          "(compact:) - Shutdown Minima. Compact the Databases if you want") {
}

std::string quit::getFullHelp() const {
    return "\nquit\n"
           "\n"
           "Shutdown Minima safely.\n"
           "\n"
           "Ensure you have a backup before shutting down.\n"
           "\n"
           "Examples:\n"
           "\n"
           "quit\n"
           "\n"
           "quit compact:true\n";
}

std::vector<std::string> quit::getValidParams() const {
    return std::vector<std::string>{ "compact" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> quit::runCommand() {
    auto ret = getJSONReply();

    bool compact = getBooleanParam("compact", false);

    if (org::minima::system::Main::getInstance() != nullptr) {
        org::minima::system::Main::getInstance()->shutdown(compact);
    }

    ret->put("message", std::string("Shutdown complete"));

    return ret;
}

org::minima::system::commands::Command* quit::getFunction() {
    return new quit();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org