#include "org/minima/system/commands/txn/txnmine.hpp"

#include <memory>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txnmine::txnmine()
    : org::minima::system::commands::Command(
          "txnmine",
          "(id:) (data:) - Mine a txn but don't post it from either ID or txnexport Data") {}

std::vector<std::string> txnmine::getValidParams() const {
    return {"id", "data"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnmine::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::txndb::TxnDB;
    using org::minima::database::userprefs::txndb::TxnRow;
    using org::minima::objects::Transaction;
    using org::minima::objects::TxPoW;
    using org::minima::objects::Witness;
    using org::minima::objects::base::MiniData;
    using org::minima::system::Main;
    using org::minima::system::brains::TxPoWGenerator;
    using org::minima::system::commands::CommandException;
    using org::minima::utils::json::JSONObject;

    // Base JSON reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Access the custom transaction DB
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();
    TxnRow* txnrow_ptr = nullptr;
    std::unique_ptr<TxnRow> txnrow_owned; // holds ownership if created from data

    // Load from ID or DATA
    if (existsParam("id")) {
        const std::string id = getParam("id");
        txnrow_ptr = db.getTransactionRow(id);
        if (!txnrow_ptr) {
            throw CommandException("Transaction not found : " + id);
        }
    } else {
        // Get HEX data
        std::unique_ptr<MiniData> dv = getDataParam("data");
        if (!dv) {
            throw CommandException("Invalid or missing data parameter");
        }
        // Convert to a TxnRow
        txnrow_owned = TxnRow::convertMiniDataVersion(*dv);
        if (!txnrow_owned) {
            throw CommandException("Failed to decode transaction data");
        }
        txnrow_ptr = txnrow_owned.get();
    }

    // Clear any previous checks
    txnrow_ptr->getTransaction().clearIsMonotonic();

    // Get the txn and witness (references, do not copy)
    Transaction& trans = txnrow_ptr->getTransaction();
    Witness& wit = txnrow_ptr->getWitness();

    // Compute the correct CoinID
    TxPoWGenerator::precomputeTransactionCoinID(trans);

    // Calculate the TransactionID
    trans.calculateTransactionID();

    // Create the TxPoW
    auto txpow_ptr = TxPoWGenerator::generateTxPoW(trans, wit);

    // Calculate derived fields (size, ids, etc.)
    txpow_ptr->calculateTXPOWID();

    // Mine it BUT don't POST it - 120000 ms timeout
    bool success = Main::getInstance()->getTxPoWMiner().MineMaxTxPoW(*txpow_ptr, 120000, false);

    if (!success) {
        throw CommandException("FAILED TO MINE txn in 120 seconds !?");
    }

    // Convert mined TxPoW to MiniData
    std::unique_ptr<MiniData> txdata = MiniData::getMiniDataVersion(*txpow_ptr);
    if (!txdata) {
        throw CommandException("Failed to serialize mined TxPoW");
    }

    // Build response
    JSONObject resp;
    resp.put("txpowid", txpow_ptr->getTxPoWID());
    resp.put("data", txdata->to0xString());
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* txnmine::getFunction() {
    return new txnmine();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org