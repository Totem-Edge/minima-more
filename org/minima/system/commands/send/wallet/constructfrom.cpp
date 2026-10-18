#include "org/minima/system/commands/send/wallet/constructfrom.hpp"

#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/command_exception.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::commands::CommandRunner;
using org::minima::system::commands::CommandException;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

namespace {
// local helper to trim whitespace
inline std::string trim_copy(const std::string& s) {
    std::string::size_type start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    std::string::size_type end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

// split by a single character delimiter
inline std::vector<std::string> split_commas(const std::string& s) {
    std::vector<std::string> out;
    std::string token;
    std::stringstream ss(s);
    while (std::getline(ss, token, ',')) {
        std::string t = trim_copy(token);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

} // anonymous namespace

constructfrom::constructfrom()
    : org::minima::system::commands::Command(
          "constructfrom",
          "[coinlist:] [script:] [toaddress:] [toamount:] [changeaddress:] [changeamount:] (tokenid:) - Create unsigned txn from a list of coins") {}

std::vector<std::string> constructfrom::getValidParams() const {
    return {
        "coinlist",
        "script",
        "toaddress",
        "toamount",
        "changeaddress",
        "changeamount",
        "tokenid"
    };
}

std::unique_ptr<JSONObject> constructfrom::runCommand() {
    // Initial reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Params
    std::string coinlist  = getParam("coinlist");
    std::string script    = getParam("script");

    std::string toaddress = getAddressParam("toaddress");
    std::unique_ptr<MiniNumber> toamount = getNumberParam("toamount");

    std::unique_ptr<MiniNumber> changeamount = getNumberParam("changeamount");
    std::string changeaddress = getAddressParam("changeaddress");

    std::string tokenid = getParam("tokenid", "0x00");

    // Random transaction id
    std::string randomid = MiniData::getRandomData(32).to0xString();

    // Create transaction
    std::shared_ptr<JSONObject> result = runCommand("txncreate id:" + randomid);

    // Add all the input coins
    std::vector<std::string> coins = split_commas(coinlist);
    for (const std::string& cc : coins) {
        std::string command = "txninput id:" + randomid + " coinid:" + cc;
        result = runCommand(command);
        bool status = false;
        try {
            status = result->getBoolean("status");
        } catch (...) {
            status = false;
        }
        if (!status) {
            // Delete transaction
            runCommand("txndelete id:" + randomid);

            // Not enough funds or some error
            std::string err;
            try {
                err = result->getString("error");
            } catch (...) {
                err = "Unknown error";
            }
            throw CommandException(err);
        }
    }

    // Add the output coin
    result = runCommand(
        "txnoutput id:" + randomid +
        " address:" + toaddress +
        " amount:" + toamount->toString() +
        " tokenid:" + tokenid);

    // Add the change if any
    if (changeamount->isMore(MiniNumber::ZERO())) {
        result = runCommand(
            "txnoutput id:" + randomid +
            " address:" + changeaddress +
            " amount:" + changeamount->toString() +
            " tokenid:" + tokenid);
    }

    // Add the scripts
    // scripts:{"<script>":""}
    {
        std::string scriptsArg = "scripts:{\"" + script + "\":\"\"}";
        runCommand("txnscript id:" + randomid + " " + scriptsArg);
    }

    // Sort the MMR
    runCommand("txnmmr id:" + randomid);

    // Export the txn (showtxn:true)
    result = runCommand("txnexport id:" + randomid + " showtxn:true");

    // Delete the temporary txn from DB
    runCommand("txndelete id:" + randomid);

    // Return response portion
    try {
        ret->put("response", result->get("response"));
    } catch (...) {
        // If no response, still return an empty object for robustness
        // Matching Java: would throw or put null; we simply omit or add empty
    }

    return ret;
}

std::shared_ptr<JSONObject> constructfrom::runCommand(const std::string& zCommand) {
    // Obtain a runner instance for this call
    auto runner = CommandRunner::getRunner();
    std::shared_ptr<JSONArray> res = runner->runMultiCommand(zCommand);

    // Extract first element as JSONObject
    std::shared_ptr<JSONObject> result;
    try {
        result = std::any_cast<std::shared_ptr<JSONObject>>(res->at(0));
    } catch (const std::bad_any_cast&) {
        try {
            // Fallback if stored by value
            JSONObject obj = std::any_cast<JSONObject>(res->at(0));
            result = std::make_shared<JSONObject>(obj);
        } catch (const std::bad_any_cast&) {
            // As a last resort, return an empty object
            result = std::make_shared<JSONObject>();
        }
    }
    return result;
}

org::minima::system::commands::Command* constructfrom::getFunction() {
    return new constructfrom();
}

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org