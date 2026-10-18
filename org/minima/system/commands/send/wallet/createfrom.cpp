#include "org/minima/system/commands/send/wallet/createfrom.hpp"

#include <any>
#include <stdexcept>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::system::commands::CommandException;
using org::minima::system::commands::CommandRunner;

createfrom::createfrom()
    : org::minima::system::commands::Command(
          "createfrom",
          "[fromaddress:] [address:] [amount:] (tokenid:) [script:] (burn:) - Create unsigned txn from a certain address") {}

std::vector<std::string> createfrom::getValidParams() const {
    return std::vector<std::string>{
        "fromaddress",
        "address",
        "amount",
        "tokenid",
        "script",
        "burn"
    };
}

std::unique_ptr<JSONObject> createfrom::runCommand() {
    // JSON reply container
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // From which address
    std::string fromaddress = getAddressParam("fromaddress");
    std::string toaddress   = getAddressParam("address");

    std::unique_ptr<MiniNumber> amount_ptr = getNumberParam("amount");
    const MiniNumber& amount = *amount_ptr;

    std::string tokenid = getAddressParam("tokenid", "0x00");

    // Get the BURN
    std::unique_ptr<MiniNumber> burn_ptr = getNumberParam("burn", MiniNumber::ZERO());
    const MiniNumber& burn = *burn_ptr;

    if (burn.isMore(MiniNumber::ZERO()) && tokenid != "0x00") {
        throw CommandException("Currently BURN on precreated transactions only works for Minima.. tokenid:0x00.. not tokens.");
    }

    // The script of the address
    std::string script = getParam("script");

    // ID of the custom transaction
    std::string randomid = MiniData::getRandomData(32).to0xString();

    // Now construct the transaction..
    std::shared_ptr<JSONObject> result = runCommandJSON("txncreate id:" + randomid);

    // Add the mounts..
    std::string command = std::string("txnaddamount id:") + randomid +
                          " burn:" + burn.toString() +
                          " fromaddress: " + fromaddress +
                          " address:" + toaddress +
                          " amount:" + amount.toString() +
                          " tokenid:" + tokenid;

    result = runCommandJSON(command);

    if (!result->getBoolean("status")) {
        // Delete transaction
        runCommandJSON("txndelete id:" + randomid);

        // Not enough funds!
        throw CommandException(result->getString("error"));
    }

    // Add the scripts..
    runCommandJSON(std::string("txnscript id:") + randomid + " scripts:{\"" + script + "\":\"\"}");

    // Sort the MMR
    runCommandJSON("txnmmr id:" + randomid);

    // Now export the txn..
    result = runCommandJSON("txnexport id:" + randomid + " showtxn:true");

    // And delete..
    runCommandJSON("txndelete id:" + randomid);

    // And return..
    ret->put("response", result->get("response"));

    return ret;
}

std::shared_ptr<JSONObject> createfrom::runCommandJSON(const std::string& zCommand) {
    // Run the command through the CommandRunner and return the first JSONObject
    std::unique_ptr<CommandRunner> runner = CommandRunner::getRunner();
    std::shared_ptr<JSONArray> res = runner->runMultiCommand(zCommand);

    if (!res || res->size() == 0) {
        throw CommandException("Empty response from CommandRunner");
    }

    const std::any& first = res->at(0);

    // Try as shared_ptr<JSONObject>
    try {
        const std::shared_ptr<JSONObject>& pobj = std::any_cast<const std::shared_ptr<JSONObject>&>(first);
        if (pobj) {
            return pobj;
        }
    } catch (const std::bad_any_cast&) {
        // fallthrough
    }

    // Try as value JSONObject
    try {
        const JSONObject& jobj = std::any_cast<const JSONObject&>(first);
        return std::make_shared<JSONObject>(jobj);
    } catch (const std::bad_any_cast&) {
        // fallthrough
    }

    // Unsupported payload type
    throw CommandException("Unexpected CommandRunner result type for first element");
}

org::minima::system::commands::Command* createfrom::getFunction() {
    return new createfrom();
}

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org