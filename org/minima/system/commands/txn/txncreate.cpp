#include "org/minima/system/commands/txn/txncreate.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <memory>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txncreate::txncreate()
    : org::minima::system::commands::Command("txncreate", "[id:] - Create a transaction") {
}

std::string txncreate::getFullHelp() const {
    return "\ntxncreate\n"
           "\n"
           "Create a custom transaction.\n"
           "\n"
           "The first step before defining the inputs and outputs.\n"
           "\n"
           "id:\n"
           "    Create an id for the transaction.\n"
           "\n"
           "Examples:\n"
           "\n"
           "txncreate id:multisig\n";
}

std::vector<std::string> txncreate::getValidParams() const {
    return std::vector<std::string>{ "id" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txncreate::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::txndb::TxnDB;

    // Access the custom transaction DB
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    // Get the transaction ID parameter (throws if missing/blank)
    std::string id = getParam("id");

    // Check if a transaction with this ID already exists
    if (db.getTransactionRow(id) != nullptr) {
        throw org::minima::system::commands::CommandException(
            std::string("Txn with this ID already exists : ") + id);
    }

    // Create the transaction
    db.createTransaction(id);

    // Build the JSON reply
    auto ret = getJSONReply();
    auto* row = db.getTransactionRow(id);
    // row should exist as we just created it
    ret->put("response", row->toJSON());

    return ret;
}

org::minima::system::commands::Command* txncreate::getFunction() {
    return new txncreate();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org