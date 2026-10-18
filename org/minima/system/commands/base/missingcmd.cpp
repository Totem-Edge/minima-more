#include "org/minima/system/commands/base/missingcmd.hpp"

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include <utility>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

missingcmd::missingcmd(const std::string& zInput, const std::string& zError)
    : org::minima::system::commands::Command("missingcmd", "")
    , mInput(zInput)
    , mError(zError) {
}

std::unique_ptr<org::minima::utils::json::JSONObject> missingcmd::runCommand() {
    auto result = getJSONReply();
    result->put("command", mInput);
    result->put("status", false);
    result->put("error", mError);
    return result;
}

org::minima::system::commands::Command* missingcmd::getFunction() {
    return new missingcmd("", "");
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org