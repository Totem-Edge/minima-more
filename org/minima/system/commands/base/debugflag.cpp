#include "org/minima/system/commands/base/debugflag.hpp"

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::utils::json::JSONObject;
using org::minima::system::params::GeneralParams;

debugflag::debugflag()
    : org::minima::system::commands::Command(
          "debugflag",
          "(activate:true|false) (var:) - Set DEBUG flag for testing code..") {}

std::vector<std::string> debugflag::getValidParams() const {
    return {"activate", "var"};
}

std::unique_ptr<JSONObject> debugflag::runCommand() {
    // Base reply JSON
    auto ret = getJSONReply();

    // Get activation parameter with default "false"
    std::string txpowon = getParam("activate", "false");

    // Set DEBUGFLAG based on parameter
    if (txpowon == "true") {
        GeneralParams::DEBUGFLAG = true;
    } else {
        GeneralParams::DEBUGFLAG = false;
    }

    // Optional variable parameter
    if (existsParam("var")) {
        GeneralParams::DEBUGVAR = getParam("var");
    }

    // Build response
    JSONObject resp;
    resp.put("debug", GeneralParams::DEBUGFLAG);
    resp.put("var", GeneralParams::DEBUGVAR);

    // Add response
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* debugflag::getFunction() {
    return new debugflag();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org