#include "org/minima/system/commands/base/coinimport.hpp"

#include <memory>
#include <utility>
#include <string>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"

#include "org/minima/system/brains/tx_po_w_searcher.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::utils::json::JSONObject;
using org::minima::database::MinimaDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMRProof;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::system::commands::CommandException;

coinimport::coinimport()
    : org::minima::system::commands::Command(
          "coinimport",
          "[data:] (track:true|false) - Import a coin, and keep tracking it") {}

std::string coinimport::getFullHelp() const {
    return "\ncoinimport\n"
           "\n"
           "Import a coin including its MMR proof.\n"
           "\n"
           "Optionally you can track the coin to add it to your relevant coins list and know when it becomes spent.\n"
           "\n"
           "Importing does not allow the spending of a coin - just the knowledge of its existence.\n"
           "\n"
           "data:\n"
           "    The data of a coin. Can be found using the 'coinexport' command.\n"
           "\n"
           "track: (optional)\n"
           "    true or false, true will create an MMR entry for the coin and add it to your relevant coins.\n"
           "\n"
           "Examples:\n"
           "\n"
           "coinimport data:0x00000..\n";
}

std::vector<std::string> coinimport::getValidParams() const {
    return std::vector<std::string>{ "data", "track" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> coinimport::runCommand() {
    auto ret = getJSONReply();

    // Mandatory parameter "data"
    std::string data = getParam("data");

    // Optional parameter "track" - defaults to true in Java.
    const bool track = getBooleanParam("track", true);

    // Convert to a CoinProof from MiniData
    MiniData md(data);
    std::unique_ptr<CoinProof> newcoinproof = CoinProof::convertMiniDataVersion(md);
    if (!newcoinproof) {
        throw CommandException("Invalid CoinProof data");
    }

    // Get the coin
    Coin& newcoin = newcoinproof->getCoin();

    // Must be unspent
    if (newcoin.getSpent()) {
        throw CommandException("Coin is spent. Can only import UNSPENT coins.");
    }

    // Get the tip
    auto db = MinimaDB::getDB();
    auto& tree = db->getTxPoWTree();
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();
    if (!tip) {
        throw CommandException("No chain tip available");
    }

    // Do we already have it: build a current proof from tip to the coin entry number
    MMRProof checkproof = tip->getMMR().getProofToPeak(newcoin.getMMREntryNumber());

    // Build a CoinProof from that (using the same Coin instance)
    std::shared_ptr<Coin> cp_coin_sp = newcoinproof->getCoinPtr();
    std::shared_ptr<MMRProof> cp_proof_sp = std::make_shared<MMRProof>(checkproof);
    CoinProof currentproof(cp_coin_sp, cp_proof_sp);

    // Build leaf data for validation at tip
    Coin& txcoin_current = currentproof.getCoin();
    std::unique_ptr<MMRData> mmrcoin_current =
        MMRData::CreateMMRDataLeafNode(txcoin_current, txcoin_current.getAmount());

    bool currentvalid = tip->getMMR().checkProofTimeValid(
        newcoin.getMMREntryNumber(),
        *mmrcoin_current,
        currentproof.getMMRProof());

    if (currentvalid) {
        // Get the tree node that contains this coin
        std::shared_ptr<TxPoWTreeNode> node =
            TxPoWSearcher::getTreeNodeForCoin(newcoin.getCoinID());
        if (node) {
            // Is it already relevant?
            const MMREntryNumber entryNum = newcoin.getMMREntryNumber();
            if (node->isRelevantEntry(entryNum)) {
                throw CommandException("Attempting to add relevant coin we already have");
            }

            // Add to relevant entries and recalculate, matching Java behaviour.
            node->addRelevantCoin(entryNum);
            node->calculateRelevantCoins();

            ret->put("response", newcoinproof->toJSON());
            return ret;
        }
    }

    // Validate the provided proof against the tip
    Coin& txcoin = newcoinproof->getCoin();
    std::unique_ptr<MMRData> mmrcoin =
        MMRData::CreateMMRDataLeafNode(txcoin, txcoin.getAmount());

    bool valid = tip->getMMR().checkProofTimeValid(
        newcoin.getMMREntryNumber(),
        *mmrcoin,
        newcoinproof->getMMRProof());

    if (!valid) {
        throw CommandException("Invalid MMR Proof");
    }

    // Block time of the proof
    const MiniNumber& coinblock = newcoinproof->getMMRProof().getBlockTime();

    // Find the tree node at that block time
    std::shared_ptr<TxPoWTreeNode> treenode = tip->getPastNode(MiniNumber(coinblock));
    if (!treenode) {
        throw CommandException("TreeNode at Blocktime not found (proof too old): " + coinblock.toString());
    }

    // Save old root (unique_ptr to avoid copying non-copyable MMRData)
    std::unique_ptr<MMRData> oldroot = treenode->getMMR().getRoot();

    // MMR data from the proof
    std::unique_ptr<MMRData> mmrdata = newcoinproof->getMMRData();

    // Update the MMR with the provided proof/data
    treenode->getMMR().setFinalized(false);
    treenode->getMMR().updateEntry(newcoin.getMMREntryNumber(),
                                    newcoinproof->getMMRProof(),
                                    *mmrdata);
    treenode->getMMR().finalizeSet();

    // Add to all coins on the node, and (optionally) to the relevant lists,
    // matching Java behaviour.
    treenode->addCoin(newcoin);
    if (track) {
        treenode->addRelevantCoin(newcoin.getMMREntryNumber());
    }
    treenode->calculateRelevantCoins();

    // New root and equality check
    std::unique_ptr<MMRData> newroot = treenode->getMMR().getRoot();
    if (!newroot->isEqual(*oldroot)) {
        JSONObject errjson = newcoinproof->toJSON();
        throw CommandException(std::string("SERIOUS ERROR : MMR root different after adding coin.. ") + errjson.toString());
    }

    ret->put("response", newcoinproof->toJSON());
    return ret;
}

org::minima::system::commands::Command* coinimport::getFunction() {
    return new coinimport();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org