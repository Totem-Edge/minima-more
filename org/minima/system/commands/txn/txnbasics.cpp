#include "org/minima/system/commands/txn/txnbasics.hpp"

#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Assumed project utility matching Java's txnutils in the same package
#include "org/minima/system/commands/txn/txnutils.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txnbasics::txnbasics()
    : org::minima::system::commands::Command(
          "txnbasics",
          "[id:] - Automatically set the MMR proofs and scripts for a txn") {}

std::string txnbasics::getFullHelp() const {
    return "\ntxnbasics\n"
           "\n"
           "Automatically set the MMR proofs and scripts for a transaction.\n"
           "\n"
           "Only run this when a transaction is ready to be posted.\n"
           "\n"
           "id:\n"
           "    The id of the transaction.\n"
           "\n"
           "Examples:\n"
           "\n"
           "txnbasics id:simpletxn\n";
}

std::vector<std::string> txnbasics::getValidParams() const {
    return std::vector<std::string>{ "id" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnbasics::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::txndb::TxnDB;
    using org::minima::database::userprefs::txndb::TxnRow;
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;
    using org::minima::system::brains::TxPoWGenerator;
    using org::minima::system::commands::CommandException;

    // Prepare reply
    std::unique_ptr<org::minima::utils::json::JSONObject> ret = getJSONReply();

    // Access transaction DB
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    // Get transaction ID param
    const std::string id = getParam("id");

    // Load the transaction row
    TxnRow* txnrow = db.getTransactionRow(id);
    if (!txnrow) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }

    // Extract Transaction and Witness
    Transaction& trans = txnrow->getTransaction();
    Witness& wit       = txnrow->getWitness();

    // Set MMR data and Scripts for coins you have
    // Note: relies on project-provided utility matching Java txnutils.setMMRandScripts
    org::minima::system::commands::txn::txnutils::setMMRandScripts(trans, wit, false);

    // Compute correct CoinID for outputs based on inputs
    TxPoWGenerator::precomputeTransactionCoinID(trans);

    // Calculate the TransactionID
    trans.calculateTransactionID();

    // Build response
    ret->put("response", txnrow->toJSON());

    return ret;
}

org::minima::system::commands::Command* txnbasics::getFunction() {
    return new txnbasics();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org