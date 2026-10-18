#include "org/minima/system/commands/txn/txnclear.hpp"

#include <memory>
#include <string>
#include <vector>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Include full definitions to call member functions on Transaction and Witness
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txnclear::txnclear()
    : org::minima::system::commands::Command(
          "txnclear",
          "[id:] (scripts:) (mmr:) (signatures:) - Clear the Witness data") {}

std::string txnclear::getFullHelp() const {
    return std::string("\ntxnclear\n")
        + "\n"
        + "Clear the Witness data - signatures, mmr proofs and script proofs.\n"
        + "\n"
        + "id:\n"
        + "    The id of the transaction to clear.\n"
        + "\n"
        + "scripts:\n"
        + "    Clear the scripts (default : true).\n"
        + "\n"
        + "mmr:\n"
        + "    Clear the MMR proofs (default : true).\n"
        + "\n"
        + "signatures:\n"
        + "    Clear the signatures (default : true).\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "txnclear id:multisig\n";
}

std::vector<std::string> txnclear::getValidParams() {
    return std::vector<std::string>{ "id", "scripts", "mmr", "signatures" };
}

std::vector<std::string> txnclear::getValidParams() const {
    return std::vector<std::string>{ "id", "scripts", "mmr", "signatures" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnclear::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::txndb::TxnDB;
    using org::minima::database::userprefs::txndb::TxnRow;
    using org::minima::system::commands::CommandException;
    using org::minima::utils::json::JSONObject;

    // Prepare the standard JSON reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Access the custom transaction DB
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    // Parameters
    std::string id   = getParam("id");
    bool script      = getBooleanParam("scripts", true);
    bool mmr         = getBooleanParam("mmr", true);
    bool sigs        = getBooleanParam("signatures", true);

    // Retrieve the transaction row
    TxnRow* txnrow = db.getTransactionRow(getParam("id"));
    if (txnrow == nullptr) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }

    // Clear transaction monotonicity flag
    txnrow->getTransaction().clearIsMonotonic();

    // Clear requested witness data
    if (script) {
        txnrow->getWitness().clearScriptProofs();
    }
    if (mmr) {
        txnrow->getWitness().clearCoinProofs();
    }
    if (sigs) {
        txnrow->getWitness().clearSignatures();
    }

    // Build response
    JSONObject resp; // created as in Java but not used directly
    ret->put("response", txnrow->toJSON());

    return ret;
}

org::minima::system::commands::Command* txnclear::getFunction() {
    return new txnclear();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org