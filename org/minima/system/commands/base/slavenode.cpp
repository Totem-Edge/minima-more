#include "org/minima/system/commands/base/slavenode.hpp"

#include <memory>
#include <string>
#include <vector>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

slavenode::slavenode()
    : org::minima::system::commands::Command(
          "slavenode",
          "Run in slavenode mode. Requires a master node to connect to.") {}

std::string slavenode::getFullHelp() const {
    return std::string("\nslavenode\n")
         + "\n"
         + "Connect to a master node and receive txblock messages.\n"
         + "\n"
         + "Examples:\n"
         + "\n"
         + "slavenode enable:true host:87.34.45.56:9001\n";
}

std::vector<std::string> slavenode::getValidParams() const {
    return std::vector<std::string>{ "host", "enable" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> slavenode::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::UserDB;
    using org::minima::utils::json::JSONObject;

    // Prepare return JSON
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Access UserDB
    UserDB& udb = MinimaDB::getDB()->getUserDB();

    // Build response object
    JSONObject resp;

    if (existsParam("enable")) {
        // A restart is required for changes to take effect
        resp.put("message", std::string("RESTART REQUIRED"));

        bool enable = getBooleanParam("enable");
        if (enable) {
            // Get the Host
            std::string host = getParam("host");

            // Set properties
            udb.setSlaveNode(enable, host);
        } else {
            udb.setSlaveNode(enable, std::string(""));
        }
    }

    // Save this
    MinimaDB::getDB()->saveUserDB();

    // Get current details
    bool slaveenable = udb.isSlaveNode();
    std::string masternode = udb.getSlaveNodeHost();

    resp.put("enabled", slaveenable);
    resp.put("master", masternode);

    // Add response
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* slavenode::getFunction() {
    return new slavenode();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org