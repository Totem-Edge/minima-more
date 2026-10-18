#include "org/minima/system/commands/txn/txnutils.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/keys/signature.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/params/global_params.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::database::wallet::Wallet;
using org::minima::objects::Transaction;
using org::minima::objects::Witness;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::ScriptProof;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMRProof;
using org::minima::system::Main;
using org::minima::system::brains::TxPoWMiner;
using org::minima::system::brains::TxPoWGenerator;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::system::commands::CommandException;
using org::minima::system::params::GlobalParams;

void txnutils::setMMRandScripts(Transaction& zTransaction, Witness& zWitness) {
    setMMRandScripts(zTransaction, zWitness, true);
}

void txnutils::setMMRandScripts(Transaction& zTransaction, Witness& zWitness, bool zExitOnFail) {
    // get the tip
    auto& tree = MinimaDB::getDB()->getTxPoWTree();
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    // Collect all the input coins from the transaction
    const auto& baseinputs = zTransaction.getAllInputs();

    // Build the actual inputs list (resolve floating or get current by CoinID)
    std::vector<std::shared_ptr<Coin>> inputs;
    inputs.reserve(baseinputs.size());

    for (const auto& cc_uptr : baseinputs) {
        const Coin& cc = *cc_uptr;
        if (cc.getCoinID().isEqual(Coin::COINID_ELTOO)) {
            // Floating: get the most recent coin matching criteria
            std::shared_ptr<Coin> floater = TxPoWSearcher::getFloatingCoin(
                tip, cc.getAmount(), cc.getAddress(), cc.getTokenID());

            if (!floater) {
                if (zExitOnFail) {
                    throw CommandException(std::string("Could not find valid unspent coin for ") + cc.toString());
                }
                // else skip
            } else {
                inputs.push_back(floater);
            }
        } else {
            // Resolve coin by CoinID (could be a pre-made coin)
            std::shared_ptr<Coin> current = TxPoWSearcher::searchCoin(cc.getCoinID());
            if (!current) {
                if (zExitOnFail) {
                    throw CommandException(std::string("Coin with CoinID not found : ") + cc.getCoinID().to0xString());
                }
                // else skip
            } else {
                inputs.push_back(current);
            }
        }
    }

    // Min depth of a coin
    MiniNumber minblock = MiniNumber::ZERO();

    // Determine min depth across inputs
    for (const auto& input : inputs) {
        if (input->getBlockCreated().isMore(minblock)) {
            minblock = input->getBlockCreated();
        }
    }

    // Determine which historical block node to use for MMR proofs
    MiniNumber currentblock = tip->getBlockNumber();
    MiniNumber blockdiff = currentblock.sub(minblock);
    if (blockdiff.isMore(GlobalParams::MINIMA_MMR_PROOF_HISTORY)) {
        blockdiff = GlobalParams::MINIMA_MMR_PROOF_HISTORY;
    }

    std::shared_ptr<TxPoWTreeNode> mmrnode =
        tip->getPastNode(tip->getBlockNumber().sub(blockdiff));
    if (!mmrnode) {
        throw std::runtime_error("Not enough blocks in chain to make valid MMR Proofs..");
    }

    // Main Wallet
    Wallet& walletdb = MinimaDB::getDB()->getWallet();

    // Add the MMR proofs and script proofs for the inputs
    for (const auto& input : inputs) {
        // MMR proof for this input
        MMRProof proof = mmrnode->getMMR().getProofToPeak(input->getMMREntryNumber());

        // Create and add CoinProof
        auto cptr = std::make_unique<CoinProof>(input, std::make_shared<MMRProof>(proof));
        zWitness.addCoinProof(std::move(cptr));

        // Add the script proof (if available), else throw on request
        std::string scraddress = input->getAddress().to0xString();
        std::unique_ptr<ScriptRow> srow = walletdb.getScriptFromAddress(scraddress);
        if (!srow) {
            if (zExitOnFail) {
                throw std::runtime_error(std::string("SERIOUS ERROR script missing for simple address : ") + scraddress);
            }
            // else skip script proof
        } else {
            auto pscr = std::make_unique<ScriptProof>(srow->getScript());
            zWitness.addScript(std::move(pscr));
        }
    }
}

void txnutils::setMMRandScripts(const Coin& zCoin, Witness& zWitness) {
    // get the tip
    auto& tree = MinimaDB::getDB()->getTxPoWTree();
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    // Min depth of the coin
    MiniNumber minblock = MiniNumber::ZERO();
    if (zCoin.getBlockCreated().isMore(minblock)) {
        minblock = zCoin.getBlockCreated();
    }

    // Determine which historical block node to use for MMR proof
    MiniNumber currentblock = tip->getBlockNumber();
    MiniNumber blockdiff = currentblock.sub(minblock);
    if (blockdiff.isMore(GlobalParams::MINIMA_MMR_PROOF_HISTORY)) {
        blockdiff = GlobalParams::MINIMA_MMR_PROOF_HISTORY;
    }

    std::shared_ptr<TxPoWTreeNode> mmrnode =
        tip->getPastNode(tip->getBlockNumber().sub(blockdiff));
    if (!mmrnode) {
        throw std::runtime_error("Not enough blocks in chain to make valid MMR Proofs..");
    }

    // Main Wallet
    Wallet& walletdb = MinimaDB::getDB()->getWallet();

    // MMR proof
    MMRProof proof = mmrnode->getMMR().getProofToPeak(zCoin.getMMREntryNumber());

    // CoinProof: deep copy coin then move into shared_ptr
    auto ucoin = zCoin.deepCopy();                 // unique_ptr<Coin>
    std::shared_ptr<Coin> coinsptr(std::move(ucoin)); // shared_ptr from unique_ptr

    auto cproof = std::make_unique<CoinProof>(coinsptr, std::make_shared<MMRProof>(proof));
    zWitness.addCoinProof(std::move(cproof));

    // Script proof (must exist)
    std::string scraddress = zCoin.getAddress().to0xString();
    std::unique_ptr<ScriptRow> srow = walletdb.getScriptFromAddress(scraddress);
    if (!srow) {
        throw CommandException(std::string("SERIOUS ERROR script missing for simple address : ") + scraddress);
    }
    auto pscr = std::make_unique<ScriptProof>(srow->getScript());
    zWitness.addScript(std::move(pscr));
}

TxnRow txnutils::createBurnTransaction(const std::vector<std::string>& zExcludeCoins,
                                       const MiniData& zLinkTransactionID,
                                       const MiniNumber& zAmount) {
    return createBurnTransaction(zExcludeCoins, zLinkTransactionID, zAmount, true);
}

TxnRow txnutils::createBurnTransaction(const std::vector<std::string>& zExcludeCoins,
                                       const MiniData& zLinkTransactionID,
                                       const MiniNumber& zAmount,
                                       bool zSign) {
    // The full TxnRow
    TxnRow txnrow("temp",
                  std::make_unique<Transaction>(),
                  std::make_unique<Witness>());

    // Get the DBs
    TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
    TxPoWMiner* txminer = &Main::getInstance()->getTxPoWMiner();
    Wallet& walletdb = MinimaDB::getDB()->getWallet();
    auto& tree = MinimaDB::getDB()->getTxPoWTree();
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    // Sending/Burning amount
    MiniNumber sendamount = zAmount;

    // Build list of relevant coins (Minima token "0x00", simple only)
    std::vector<std::shared_ptr<Coin>> relcoins =
        TxPoWSearcher::getRelevantUnspentCoins(tip, "0x00", true);

    // Current accumulated amount and selected coins
    MiniNumber currentamount = MiniNumber::ZERO();
    std::vector<std::shared_ptr<Coin>> currentcoins;

    for (const auto& coin : relcoins) {
        std::string coinidstr = coin->getCoinID().to0xString();

        // Excluded?
        if (std::find(zExcludeCoins.begin(), zExcludeCoins.end(), coinidstr) != zExcludeCoins.end()) {
            continue;
        }

        // Already being used by a transaction that is being mined?
        if (txminer && txminer->checkForMiningCoin(coinidstr)) {
            continue;
        }

        // Already in mempool?
        if (txpdb.checkMempoolCoins(coin->getCoinID())) {
            continue;
        }

        // Add this coin
        currentcoins.push_back(coin);

        // Accumulate amount
        currentamount = currentamount.add(coin->getAmount());

        // Have enough?
        if (currentamount.isMoreEqual(sendamount)) {
            break;
        }
    }

    // Did we add enough?
    if (currentamount.isLess(sendamount)) {
        throw CommandException("Not enough funds / coins for the burn..");
    }

    // Calculate change
    MiniNumber change = currentamount.sub(sendamount);

    // Construct the transaction
    Transaction& transaction = txnrow.getTransaction();
    Witness& witness = txnrow.getWitness();

    // Min depth of an input coin
    MiniNumber minblock = MiniNumber::ZERO();

    // Add the inputs
    for (const auto& input : currentcoins) {
        // Transaction holds unique_ptr<Coin>, so deep copy the chain coin
        transaction.addInput(input->deepCopy());

        // Track deepest block created
        if (input->getBlockCreated().isMore(minblock)) {
            minblock = input->getBlockCreated();
        }
    }

    // Determine which historical node for MMR proofs
    MiniNumber currentblock = tip->getBlockNumber();
    MiniNumber blockdiff = currentblock.sub(minblock);
    if (blockdiff.isMore(GlobalParams::MINIMA_MMR_PROOF_HISTORY)) {
        blockdiff = GlobalParams::MINIMA_MMR_PROOF_HISTORY;
    }

    std::shared_ptr<TxPoWTreeNode> mmrnode =
        tip->getPastNode(tip->getBlockNumber().sub(blockdiff));
    if (!mmrnode) {
        throw CommandException("Not enough blocks in chain to make valid MMR Proofs..");
    }

    // Required signatures (unique pubkeys)
    std::vector<std::string> reqsigs;

    // Add MMR proofs and script proofs for the selected inputs
    for (const auto& input : currentcoins) {
        // MMR proof
        MMRProof proof = mmrnode->getMMR().getProofToPeak(input->getMMREntryNumber());

        // CoinProof
        auto cp = std::make_unique<CoinProof>(input, std::make_shared<MMRProof>(proof));
        witness.addCoinProof(std::move(cp));

        // Script proof
        std::string scraddress = input->getAddress().to0xString();
        std::unique_ptr<ScriptRow> srow = walletdb.getScriptFromAddress(scraddress);
        if (!srow) {
            throw CommandException(std::string("SERIOUS ERROR script missing for simple address : ") + scraddress);
        }
        auto pscr = std::make_unique<ScriptProof>(srow->getScript());
        witness.addScript(std::move(pscr));

        // Add required pubkey if not already present
        const std::string pubkey = srow->getPublicKey();
        if (std::find(reqsigs.begin(), reqsigs.end(), pubkey) == reqsigs.end()) {
            reqsigs.push_back(pubkey);
        }
    }

    // Validate Minima amount
    if (!sendamount.isValidMinimaValue()) {
        throw CommandException(std::string("Invalid Minima amount to send.. ") + sendamount.toString());
    }

    // Change output if necessary
    if (change.isMore(MiniNumber::ZERO())) {
        std::unique_ptr<ScriptRow> newwalletaddress =
            MinimaDB::getDB()->getWallet().getDefaultAddress();
        MiniData chgaddress(newwalletaddress->getAddress());

        MiniNumber changeamount = change;

        // Change coin does not keep the state
        auto changecoin = std::make_unique<Coin>(Coin::COINID_OUTPUT, chgaddress, changeamount, Token::TOKENID_MINIMA, false);

        // Add change output
        transaction.addOutput(std::move(changecoin));
    }

    // Set link hash (burn transaction)
    transaction.setLinkHash(zLinkTransactionID);

    // Precompute CoinIDs from first input
    TxPoWGenerator::precomputeTransactionCoinID(transaction);

    // Calculate TransactionID
    transaction.calculateTransactionID();

    // Optionally sign
    if (zSign) {
        for (const std::string& pubk : reqsigs) {
            std::unique_ptr<org::minima::objects::keys::Signature> signature =
                walletdb.signData(pubk, transaction.getTransactionID());
            witness.addSignature(std::move(signature));
        }
    }

    return txnrow;
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org