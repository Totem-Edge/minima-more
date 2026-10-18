#include "org/minima/system/commands/txn/txnpost.hpp"

#include <stdexcept>
#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"

#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/coin.hpp"

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"

#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/main.hpp"

// Assume TxPoWMiner header is available to call mining methods
#include "org/minima/system/brains/tx_po_w_miner.hpp"

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Utilities used by Java version; assumed to exist in project
#include "org/minima/system/commands/txn/txnutils.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txnpost::txnpost()
    : org::minima::system::commands::Command(
          "txnpost",
          "[id:] (auto:true) (burn:) (mine:) (txndelete:)- Post a transaction. Automatically set the Scripts and MMR") {}

std::string txnpost::getFullHelp() const {
    return std::string("\ntxnpost\n"
                       "\n"
                       "Post a transaction. Automatically set the Scripts and MMR proofs.\n"
                       "\n"
                       "Optionally set a burn for the transaction.\n"
                       "\n"
                       "id:\n"
                       "    The id of the transaction.\n"
                       "\n"
                       "auto: (optional)\n"
                       "    Set the scripts and MMR proofs for the transaction.\n"
                       "\n"
                       "burn: (optional)\n"
                       "    Amount in Minima to burn with the transaction.\n"
                       "\n"
                       "mine: (optional)\n"
                       "    true or false - should you mine the transaction immediately.\n"
                       "\n"
                       "txndelete: (optional)\n"
                       "    true or false - delete this txn after posting.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "txnpost id:simpletxn\n"
                       "\n"
                       "txnpost id:simpletxn auto:true burn:0.1\n"
                       "\n"
                       "txnpost id:multisig burn:0.1\n");
}

std::vector<std::string> txnpost::getValidParams() const {
    return std::vector<std::string>{ "id", "auto", "burn", "mine", "txndelete" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnpost::runCommand() {
    using org::minima::objects::TxPoW;
    using org::minima::objects::base::MiniNumber;
    using org::minima::system::commands::CommandException;

    auto ret = getJSONReply();

    // Get the details
    std::string id = getParam("id");
    std::unique_ptr<MiniNumber> burnptr = getNumberParam("burn", MiniNumber::ZERO());
    const MiniNumber& burn = *burnptr;
    bool autoparam = getBooleanParam("auto", false);

    // Are we Mining synchronously
    bool minesync = getBooleanParam("mine", false);

    // Post the Txn..
    auto txpow_ptr = postTxn(id, burn, autoparam, minesync);

    // Are we auto-deleting
    bool autodelete = getBooleanParam("txndelete", false);
    if (autodelete) {
        org::minima::database::userprefs::txndb::TxnDB& db =
            org::minima::database::MinimaDB::getDB()->getCustomTxnDB();

        bool found = db.deleteTransaction(id);
        (void)found; // Java ignores result
    }

    // Add to response..
    ret->put("response", txpow_ptr->toJSON());

    return ret;
}

org::minima::system::commands::Command* txnpost::getFunction() {
    return new txnpost();
}

std::unique_ptr<org::minima::objects::TxPoW> txnpost::postTxn(const std::string& zID,
                                              const org::minima::objects::base::MiniNumber& zBurn,
                                              bool zAuto,
                                              bool zMineSync) {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::txndb::TxnDB;
    using org::minima::database::userprefs::txndb::TxnRow;
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;
    using org::minima::objects::CoinProof;
    using org::minima::objects::TxPoW;
    using org::minima::objects::base::MiniNumber;
    using org::minima::objects::base::MiniData;
    using org::minima::system::commands::CommandException;

    // Get the TXN DB
    TxnDB* db = &MinimaDB::getDB()->getCustomTxnDB();

    // The transaction
    const std::string id = zID;
    MiniNumber burn = zBurn;
    if (burn.isLess(MiniNumber::ZERO())) {
        throw CommandException(std::string("Cannot have negative burn ") + burn.toString());
    }

    // Get the row..
    TxnRow* txnrow = db->getTransactionRow(id);
    if (txnrow == nullptr) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }

    // Get the Transaction and Witness
    Transaction& trans = txnrow->getTransaction();
    Witness& wit       = txnrow->getWitness();

    // Clear any previous checks..
    txnrow->getTransaction().clearIsMonotonic();

    // Set the scripts and MMR
    if (zAuto) {
        // Set the MMR data and Scripts
        org::minima::system::commands::txn::txnutils::setMMRandScripts(trans, wit);
    }

    // Compute the correct CoinID
    org::minima::system::brains::TxPoWGenerator::precomputeTransactionCoinID(trans);

    // Calculate the TransactionID..
    trans.calculateTransactionID();

    // The final TxPoW
    std::unique_ptr<org::minima::objects::TxPoW> txpow_ptr;

    // Is there a burn
    if (burn.isMore(MiniNumber::ZERO())) {
        // Get all the used coins..
        std::vector<std::string> addedcoinid;
        auto& coins = wit.getAllCoinProofs();
        addedcoinid.reserve(coins.size());
        for (const auto& cp_up : coins) {
            const CoinProof& cp = *cp_up;
            // coin -> coinID -> to0xString
            addedcoinid.push_back(cp.getCoin().getCoinID().to0xString());
        }

        // Create a Burn Transaction (unique_ptr<TxnRow>)
        auto burntxn = org::minima::system::commands::txn::txnutils::createBurnTransaction(
            addedcoinid, trans.getTransactionID(), burn);

        // Now create a complete TxPOW (use pointer args for burn variant)
        txpow_ptr = org::minima::system::brains::TxPoWGenerator::generateTxPoW(
            trans, wit, &burntxn.getTransaction(), &burntxn.getWitness());
    } else {
        // Now create the TxPoW
        txpow_ptr = org::minima::system::brains::TxPoWGenerator::generateTxPoW(trans, wit);
    }

    // Calculate the size / ID
    txpow_ptr->calculateTXPOWID();

    // Sync or Async mining..
    if (zMineSync) {
        bool success = org::minima::system::Main::getInstance()
                           ->getTxPoWMiner()
                           .MineMaxTxPoW(*txpow_ptr, 120000);

        if (!success) {
            throw CommandException("FAILED TO MINE txn in 120 seconds !?");
        }
    } else {
        org::minima::system::Main::getInstance()->getTxPoWMiner().mineTxPoWAsync(*txpow_ptr);
    }

    return txpow_ptr;
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org