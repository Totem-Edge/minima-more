#include "org/minima/system/commands/base/state.hpp"

#include "org/minima/database/state_report.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

state::state()
    : org::minima::system::commands::Command(
          "state",
          "(action:verify) - Verify archive, cascade, tree and MMR consistency") {}

std::string state::getFullHelp() const {
    return std::string(
        "\nstate\n"
        "\n"
        "Read-only consistency check across the coupled stores.\n"
        "\n"
        "action:\n"
        "    verify : Check archive contiguity/range, cascade validity, tree\n"
        "             alignment and the archive gap. Returns ok:true/false and\n"
        "             an issues string. Exit code via the RPC status field.\n"
        "\n"
        "Examples:\n"
        "\n"
        "state action:verify\n");
}

std::vector<std::string> state::getValidParams() const {
    return std::vector<std::string>{"action"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> state::runCommand() {
    auto ret = std::make_unique<org::minima::utils::json::JSONObject>();
    org::minima::database::StateReport::build(*ret);
    return ret;
}

org::minima::system::commands::Command* state::getFunction() {
    return new state();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
