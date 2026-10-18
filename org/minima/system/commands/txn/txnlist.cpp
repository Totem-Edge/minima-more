#include "org/minima/system/commands/txn/txnlist.hpp"

#include <utility>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No Windows-specific code required here, but block kept for portability clarity.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::txndb::TxnDB;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::system::commands::CommandException;

txnlist::txnlist()
    : org::minima::system::commands::Command(
          "txnlist",
          "(id:) (transactiononly:) - List current custom transactions") {
}

std::string txnlist::getFullHelp() const {
    return std::string()
        + "\ntxnlist\n"
        + "\n"
        + "List your custom transactions. Includes previously posted transactions.\n"
        + "\n"
        + "Returns the full details of transactions.\n"
        + "\n"
        + "id: (optional)\n"
        + "    The id of a single transaction to list.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "txnlist\n"
        + "\n"
        + "txnlist id:multisig\n";
}

std::vector<std::string> txnlist::getValidParams() const {
    return std::vector<std::string>{ "id", "transactiononly" };
}

std::unique_ptr<JSONObject> txnlist::runCommand() {
    auto ret = getJSONReply();

    // Access TxnDB
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    // Parameters
    std::string id = getParam("id", "");
    bool transonly = getBooleanParam("transactiononly", false);

    if (id.empty()) {
        // List all transactions
        auto& txns = db.listTxns();

        JSONArray arr;
        for (const auto& txnptr : txns) {
            if (txnptr) {
                // toJSON(!transonly) mirrors Java logic
                JSONObject obj = txnptr->toJSON(!transonly);
                arr.add(obj);
            }
        }

        ret->put("response", arr);
    } else {
        // Single transaction by id
        TxnRow* txnrow = db.getTransactionRow(id);
        if (!txnrow) {
            throw CommandException("Transaction not found : " + id);
        }

        ret->put("response", txnrow->toJSON(!transonly));
    }

    return ret;
}

org::minima::system::commands::Command* txnlist::getFunction() {
    return new txnlist();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org