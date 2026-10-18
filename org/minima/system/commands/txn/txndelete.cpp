#include "org/minima/system/commands/txn/txndelete.hpp"

#include <utility>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txndelete::txndelete()
    : org::minima::system::commands::Command("txndelete", "[id:] - Delete this custom transaction") {
}

std::string txndelete::getFullHelp() const {
    return "\ntxndelete\n"
           "\n"
           "Delete a previously created custom transaction.\n"
           "\n"
           "id:\n"
           "    The id of the transaction to delete or 'all' to clear ALL transactions.\n"
           "\n"
           "Examples:\n"
           "\n"
           "txndelete id:multisig\n"
           "\n"
           "txndelete id:all\n"
           "\n";
}

std::vector<std::string> txndelete::getValidParams() const {
    return std::vector<std::string>{ "id" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txndelete::runCommand() {
    // Create the standard JSON reply object
    std::unique_ptr<org::minima::utils::json::JSONObject> ret = getJSONReply();

    // Access the TxnDB through the singleton MinimaDB
    org::minima::database::userprefs::txndb::TxnDB& db =
        org::minima::database::MinimaDB::getDB()->getCustomTxnDB();

    // Retrieve required parameter 'id' (throws if missing/blank to match Java behavior)
    std::string id = getParam("id");

    bool found = true;
    if (id == "all") {
        db.clearTxns();
    } else {
        // Delete a single transaction, capturing whether it was found
        found = db.deleteTransaction(id);
    }

    if (found) {
        ret->put("response", std::string("Deleted"));
    } else {
        ret->put("response", std::string("Not found"));
    }

    return ret;
}

org::minima::system::commands::Command* txndelete::getFunction() {
    return new txndelete();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org