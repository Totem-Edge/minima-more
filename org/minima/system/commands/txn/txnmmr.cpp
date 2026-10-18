#include "org/minima/system/commands/txn/txnmmr.hpp"

#include <memory>
#include <utility>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"

// TxPoW tree and node
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

// Txn DB and rows
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"

// Objects
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"

// MMR related
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"

// Numbers and params
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/params/global_params.hpp"

// JSON
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::txndb::TxnDB;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::objects::Transaction;
using org::minima::objects::Witness;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMRProof;

txnmmr::txnmmr()
    : org::minima::system::commands::Command(
          "txnmmr",
          "[id:] - Add MMR proofs to a transaction leaving those already present") {}

std::vector<std::string> txnmmr::getValidParams() const {
    return std::vector<std::string>{ "id" };
}

// Helper to wrap an MMRProof value in a shared_ptr.
static std::shared_ptr<MMRProof> ensure_shared(MMRProof proof_val) {
    return std::make_shared<MMRProof>(std::move(proof_val));
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnmmr::runCommand() {
    // Prepare JSON reply
    auto ret = getJSONReply();

    // Access the custom transaction DB
    TxnDB* db = &MinimaDB::getDB()->getCustomTxnDB();

    // The transaction ID
    std::string id = getParam("id");

    // Get the transaction row
    TxnRow* txnrow = db->getTransactionRow(id);
    if (!txnrow) {
        throw org::minima::system::commands::CommandException("Transaction not found : " + id);
    }

    // Access Transaction and Witness
    Transaction& trans = txnrow->getTransaction();
    Witness& witness = txnrow->getWitness();

    // Current number of coin proofs
    int proofs = static_cast<int>(witness.getAllCoinProofs().size());

    // Get the tip and current block number
    auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
    MiniNumber currentblock = tip->getBlockNumber();

    // Which node are we going to use..
    MiniNumber minblock = currentblock.sub(org::minima::system::params::GlobalParams::MINIMA_MMR_PROOF_HISTORY);

    // Get all the inputs
    auto& inputs = trans.getAllInputs();

    // Add the inputs - compute minblock as the most recent block among inputs
    for (const auto& input : inputs) {
        const MiniNumber bc = input->getBlockCreated();
        if (bc.isMore(minblock)) {
            minblock = bc;
        }
    }

    // Now get that Tree node
    auto mmrnode = tip->getPastNode(minblock);
    if (!mmrnode) {
        // Chain too short to reach the requested historical block. Fall back to
        // the root (genesis) node so the proof can still be produced.
        mmrnode = MinimaDB::getDB()->getTxPoWTree().getRoot();
    }
    if (!mmrnode) {
        throw org::minima::system::commands::CommandException("Not enough blocks in chain to make valid MMR Proofs..");
    }

    // Cycle through the inputs..
    int counter = 0;
    for (const auto& input : inputs) {
        // Only add coins AFTER the current proofs
        ++counter;
        if (counter <= proofs) {
            continue;
        }

        // Get the proof from the MMR, adapt to shared_ptr
        auto& mmr = mmrnode->getMMR();
        auto proof_any = mmr.getProofToPeak(input->getMMREntryNumber());
        std::shared_ptr<MMRProof> proof_ptr = ensure_shared(std::move(proof_any));

        // Create a deep-copied Coin for the CoinProof to own via shared_ptr
        std::unique_ptr<Coin> coin_copy_unique = input->deepCopy();
        std::shared_ptr<Coin> coin_shared = std::move(coin_copy_unique); // shared_ptr from unique_ptr

        // Create the CoinProof..
        auto cp = std::make_unique<CoinProof>(coin_shared, proof_ptr);

        // Add it to the witness data
        witness.addCoinProof(std::move(cp));
    }

    // Output the current transaction row JSON
    TxnRow* updated = db->getTransactionRow(id);
    if (updated) {
        ret->put("response", updated->toJSON());
    } else {
        ret->put("response", org::minima::utils::json::JSONObject());
    }

    return ret;
}

org::minima::system::commands::Command* txnmmr::getFunction() {
    return new txnmmr();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org