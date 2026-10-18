#include "org/minima/system/commands/base/incentivecash.hpp"

#include <memory>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/r_p_c_client.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::database::MinimaDB;
using org::minima::system::params::GlobalParams;
using org::minima::utils::RPCClient;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::parser::JSONParser;

incentivecash::incentivecash()
    : Command(
          "incentivecash",
          "(uid:) - Show your rewards or specify your UserID for the Incentive Cash program") {
}

std::string incentivecash::getFullHelp() const {
    return std::string()
        + "\nincentivecash\n"
        + "\n"
        + "Returns your Incentive Program Rewards balance and full breakdown of daily, invite and community Rewards.\n"
        + "\n"
        + "Set your Incentive ID with the 'uid' parameter to start receiving daily Rewards.\n"
        + "\n"
        + "uid: (optional)\n"
        + "    Your Incentive Program ID, can be found by logging into the incentive.minima.global website.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "incentivecash\n"
        + "\n"
        + "incentivecash uid:00d11b34-7b47-45f3-775c-a37cbe4c9ff3\n";
}

std::vector<std::string> incentivecash::getValidParams() const {
    return std::vector<std::string>{ "uid" };
}

std::unique_ptr<JSONObject> incentivecash::runCommand() {
    // Create base reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Set the UID if provided
    if (existsParam("uid")) {
        std::string uid = getParam("uid");

        // Set this in the UserDB
        MinimaDB::getDB()->getUserDB().setIncentiveCashUserID(uid);

        // Save this
        MinimaDB::getDB()->saveUserDB();
    }

    // Get the User
    std::string user = MinimaDB::getDB()->getUserDB().getIncentiveCashUserID();

    // Build the details object
    JSONObject ic;
    ic.put("uid", user);

    // Make sure there is a User specified
    if (!user.empty()) {
        // Call the RPC endpoint
        std::string url = "https://incentivecash.minima.global/api/ping/" + user + "?version=" + GlobalParams::MINIMA_VERSION;
        std::string reply = RPCClient::sendPUT(url);

        // Convert response
        JSONParser parser;
        std::any parsed = parser.parse(reply);

        // Expect a JSONObject (mirror Java cast)
        bool stored = false;
        try {
            // Prefer container as shared_ptr<JSONObject>
            std::shared_ptr<JSONObject> pobj = std::any_cast<std::shared_ptr<JSONObject>>(parsed);
            if (pobj) {
                // Store by value to avoid holding shared_ptr in our JSONObject any
                ic.put("details", *pobj);
                stored = true;
            }
        } catch (const std::bad_any_cast&) {
            // fallthrough
        }

        if (!stored) {
            try {
                JSONObject jobj = std::any_cast<JSONObject>(parsed);
                ic.put("details", jobj);
                stored = true;
            } catch (const std::bad_any_cast&) {
                // Not an object - mirror Java ClassCastException semantics
                throw std::runtime_error("JSONParser.parse did not return a JSONObject");
            }
        }
    }

    ret->put("response", ic);

    return ret;
}

Command* incentivecash::getFunction() {
    return new incentivecash();
}

}
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org