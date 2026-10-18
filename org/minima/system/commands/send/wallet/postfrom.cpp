#include "org/minima/system/commands/send/wallet/postfrom.hpp"

#include <stdexcept>
#include <sstream>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

postfrom::postfrom()
    : org::minima::system::commands::Command(
          "postfrom",
          "[data:] (mine:true|false) (mmr:true|false) - Post a signfrom txn ") {}

std::vector<std::string> postfrom::getValidParams() const {
    // Mirror Java: ["data","mine","mmr"]
    return std::vector<std::string>{ "data", "mine", "mmr" };
}

std::unique_ptr<JSONObject> postfrom::runCommand() {
    // Prepare response json
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Access TxnDB
    org::minima::database::userprefs::txndb::TxnDB& db =
        org::minima::database::MinimaDB::getDB()->getCustomTxnDB();

    // Get the HEX data
    std::unique_ptr<org::minima::objects::base::MiniData> dv = getDataParam("data");

    // Convert to a TxnRow
    std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow> tx =
        org::minima::database::userprefs::txndb::TxnRow::convertMiniDataVersion(*dv);

    if (!tx) {
        throw std::runtime_error("Invalid data: cannot convert to TxnRow");
    }

    // Optional explicit id
    if (existsParam("id")) {
        tx->setID(getParam("id"));
    }

    bool sortmmr = getBooleanParam("mmr", false);

    std::string randomid = tx->getID();

    // Add to the DB (ownership transfer)
    db.addCompleteTransaction(std::move(tx));

    // Are we mining
    bool mine = getBooleanParam("mine", false);

    // Do we sort the MMR
    if (sortmmr) {
        runCommandInternal(std::string("txnmmr id:") + randomid);
    }

    // And POST!
    std::string mineStr = mine ? "true" : "false";
    std::unique_ptr<JSONObject> result =
        runCommandInternal(std::string("txnpost id:") + randomid + " mine:" + mineStr);

    // And delete..
    runCommandInternal(std::string("txndelete id:") + randomid);

    // And return..
    ret->put("response", result->get("response"));

    return ret;
}

std::unique_ptr<JSONObject> postfrom::runCommandInternal(const std::string& zCommand) {
    auto runner = org::minima::system::commands::CommandRunner::getRunner();
    std::shared_ptr<JSONArray> res = runner->runMultiCommand(zCommand);

    if (!res || res->size() == 0) {
        throw std::runtime_error("CommandRunner returned empty result for: " + zCommand);
    }

    const std::any& first = res->at(0);

    // Try several possible stored types
    // 1) shared_ptr<JSONObject>
    if (first.type() == typeid(std::shared_ptr<JSONObject>)) {
        auto sp = std::any_cast<std::shared_ptr<JSONObject>>(first);
        if (!sp) {
            throw std::runtime_error("First result is null JSONObject for: " + zCommand);
        }
        return std::make_unique<JSONObject>(*sp);
    }

    // 2) unique_ptr<JSONObject>
    if (first.type() == typeid(std::unique_ptr<JSONObject>)) {
        const auto& up = std::any_cast<const std::unique_ptr<JSONObject>&>(first);
        if (!up) {
            throw std::runtime_error("First result is null JSONObject for: " + zCommand);
        }
        return std::make_unique<JSONObject>(*up);
    }

    // 3) raw pointer JSONObject*
    if (first.type() == typeid(JSONObject*)) {
        JSONObject* ptr = std::any_cast<JSONObject*>(first);
        if (!ptr) {
            throw std::runtime_error("First result is null JSONObject for: " + zCommand);
        }
        return std::make_unique<JSONObject>(*ptr);
    }

    // 4) JSONObject by value
    if (first.type() == typeid(JSONObject)) {
        const JSONObject& obj = std::any_cast<const JSONObject&>(first);
        return std::make_unique<JSONObject>(obj);
    }

    // If none matched, attempt a generic cast to shared_ptr<void> and throw
    throw std::runtime_error("Unexpected result type in JSONArray for: " + zCommand);
}

org::minima::system::commands::Command* postfrom::getFunction() {
    return new postfrom();
}

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org