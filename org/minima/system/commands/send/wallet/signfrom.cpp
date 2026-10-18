#include "org/minima/system/commands/send/wallet/signfrom.hpp"

#include <any>
#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
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

signfrom::signfrom()
    : org::minima::system::commands::Command(
          "signfrom",
          "[data:] [privatekey:] [keyuses:] - Sign a creatfrom txn") {}

std::vector<std::string> signfrom::getValidParams() const {
    return std::vector<std::string>{ "id", "data", "privatekey", "keyuses" };
}

std::unique_ptr<JSONObject> signfrom::runCommand() {
    // JSON reply
    auto ret = getJSONReply();

    // Access TxnDB
    org::minima::database::userprefs::txndb::TxnDB& db =
        org::minima::database::MinimaDB::getDB()->getCustomTxnDB();

    // Get the HEX data
    std::unique_ptr<org::minima::objects::base::MiniData> dv_up = getDataParam("data");
    const org::minima::objects::base::MiniData& dv = *dv_up;

    // Convert to a TxnRow
    std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow> tx =
        org::minima::database::userprefs::txndb::TxnRow::convertMiniDataVersion(dv);

    // If 'id' provided, override
    if (existsParam("id")) {
        tx->setID(getParam("id"));
    }

    // Random id (actual current ID)
    std::string randomid = tx->getID();

    // Add to the DB (takes ownership)
    db.addCompleteTransaction(std::move(tx));

    // The private key we need to sign with
    std::string privatekey = getAddressParam("privatekey");
    std::unique_ptr<org::minima::objects::base::MiniNumber> keyuses = getNumberParam("keyuses");

    // Now SIGN
    {
        std::string cmd = "txnsign id:" + randomid +
                          " publickey:custom privatekey:" + privatekey +
                          " keyuses:" + keyuses->toString();
        runCommandStr(cmd);
    }

    // Now export the txn..
    std::shared_ptr<JSONObject> result = runCommandStr("txnexport id:" + randomid);

    // And delete..
    runCommandStr("txndelete id:" + randomid);

    // And return..
    if (result) {
        ret->put("response", result->get("response"));
    }

    return ret;
}

std::shared_ptr<JSONObject> signfrom::runCommandStr(const std::string& zCommand) {
    std::shared_ptr<JSONArray> res =
        org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(zCommand);

    // Expect at least one result
    if (!res || res->size() == 0) {
        return std::make_shared<JSONObject>();
    }

    const std::any& first = res->at(0);

    // Try common encodings: shared_ptr<JSONObject>, JSONObject value
    if (first.type() == typeid(std::shared_ptr<JSONObject>)) {
        return std::any_cast<std::shared_ptr<JSONObject>>(first);
    }
    if (first.type() == typeid(JSONObject)) {
        return std::make_shared<JSONObject>(std::any_cast<JSONObject>(first));
    }

    // As a fallback, return empty object
    return std::make_shared<JSONObject>();
}

org::minima::system::commands::Command* signfrom::getFunction() {
    return new signfrom();
}

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org