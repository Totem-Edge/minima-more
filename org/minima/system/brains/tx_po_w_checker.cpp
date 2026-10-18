#include "org/minima/system/brains/tx_po_w_checker.hpp"

#include <unordered_set>
#include <chrono>
#include <thread>
#include <memory>
#include <algorithm>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"

#include "org/minima/system/network/minima/relay_policy.hpp" // Assumed to be correct

#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/magic.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/state_variable.hpp"

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"

// Forward declarations for TxPoWGenerator (avoid missing header)
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace system { namespace brains {
class TxPoWGenerator {
public:
    static std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>
    getMedianTimeBlock(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zParentNode, int zBlocks);

    static org::minima::objects::base::MiniData
    getBlockDifficulty(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zParentNode);
};
} } } }

namespace org {
namespace minima {
namespace system {
namespace brains {

using org::minima::utils::MinimaLogger;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::TxPoW;
using org::minima::objects::TxBlock;
using org::minima::objects::Transaction;
using org::minima::objects::Witness;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::ScriptProof;
using org::minima::objects::Token;

using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;

using org::minima::database::txpowtree::TxPoWTreeNode;

// Static members
// NOTE: CURRENT_NETWORK is initialised from the literal "0x00" (==
// TxHeader::MAIN_NET) rather than copying TxHeader::MAIN_NET. The latter is a
// dynamically-initialised static in another translation unit; when this unit's
// static initialiser ran first it copied an as-yet-unconstructed (empty) value,
// which then rejected every block with "Wrong Block ChainID" (a latent cross-TU
// static-initialisation-order fiasco). Keep in sync with TxHeader::MAIN_NET.
org::minima::objects::base::MiniData TxPoWChecker::CURRENT_NETWORK("0x00");
org::minima::objects::base::MiniNumber TxPoWChecker::MAX_TIME_FUTURE = org::minima::objects::base::MiniNumber(1000LL * 60 * 60 * 24);

// FIX: zTxPoW is now NON-CONST
bool TxPoWChecker::checkTxPoWBlockTimed(TxPoWTreeNode* zParentNode,
                                        TxPoW& zTxPoW, // NOT CONST
                                        std::vector<TxPoW*>& zTransactions) {
    if (!zParentNode) {
        MinimaLogger::log("checkTxPoWBlockTimed with NULL parent node");
        return false;
    }
                                            
    auto tstart = std::chrono::steady_clock::now();
    bool valid  = false;
    try {
        valid = checkTxPoWBlock(zParentNode, zTxPoW, zTransactions);
    } catch (const std::exception& e) {
        MinimaLogger::log(std::string("Block failed to process : ") + e.what());
        valid = false;
    } catch (...) {
        MinimaLogger::log("Block failed to process : unknown exception");
        valid = false;
    }

    if (org::minima::system::params::GeneralParams::BLOCK_LOGS) {
        auto tend = std::chrono::steady_clock::now();
        auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(tend - tstart).count();
        MinimaLogger::log(std::string("[VALID:") + (valid ? "true" : "false") + "] Block checker time : " +
                          std::to_string(ms) + "ms @ " + zTxPoW.getBlockNumber().toString() +
                          " " + zTxPoW.getTxPoWID());
    }

    return valid;
}

// FIX: zTxPoW is now NON-CONST
bool TxPoWChecker::checkTxPoWBlock(TxPoWTreeNode* zParentNode,
                                   TxPoW& zTxPoW, // NOT CONST
                                   std::vector<TxPoW*>& zTransactions) {
    if (!zParentNode) {
        MinimaLogger::log("checkTxPoWBlock with NULL parent node");
        return false;
    }

    try {
        // Check ChainID
        if (!zTxPoW.getChainID().isEqual(TxPoWChecker::CURRENT_NETWORK)) {
            MinimaLogger::log("Invalid Block ChainID! " + zTxPoW.getChainID().to0xString() + " " + zTxPoW.getTxPoWID());
            return false;
        }

        // Check Block Number
        if (!zTxPoW.getBlockNumber().isEqual(zParentNode->getBlockNumber().increment())) {
            MinimaLogger::log("Invalid TxPoW block with wrong blocknumber " + zTxPoW.getTxPoWID());
            return false;
        }

        // Check Parents
        if (!checkParents(*zParentNode, zTxPoW)) {
            MinimaLogger::log("Invalid TxPoW Super Parents " + zTxPoW.getTxPoWID());
            return false;
        }

        // Check TimeMilli window vs median
        std::shared_ptr<TxPoWTreeNode> median = org::minima::system::brains::TxPoWGenerator::getMedianTimeBlock(
            zParentNode->shared_from_this(),
            org::minima::system::params::GlobalParams::MEDIAN_BLOCK_CALC * 2);
        if (!median) {
            MinimaLogger::log("Failed to get median time block");
            return false;
        }
        MiniNumber maxtime = median->getTxPoW().getTimeMilli().add(MAX_TIME_FUTURE);
        if (zTxPoW.getTimeMilli().isLess(median->getTxPoW().getTimeMilli())) {
            MinimaLogger::log("Invalid TxPoW TimeMilli less than median 1 hr back " + zTxPoW.getTxPoWID());
            return false;
        } else if (zTxPoW.getTimeMilli().isMore(maxtime)) {
            MinimaLogger::log("Invalid TxPoW TimeMilli more than 24 hrs in future " + zTxPoW.getTxPoWID());
            return false;
        }

        // Check block difficulty
        MiniData blockdifficulty = org::minima::system::brains::TxPoWGenerator::getBlockDifficulty(zParentNode->shared_from_this());
        
        // DEBUG
        // MinimaLogger::log("=== BLOCK DIFFICULTY CHECK ===");
        // MinimaLogger::log("Parent Block: " + zParentNode->getBlockNumber().toString());
        // MinimaLogger::log("Parent TxPoWID: " + zParentNode->getTxPoW().getTxPoWID());
        // MinimaLogger::log("Expected Difficulty: " + blockdifficulty.to0xString());
        // MinimaLogger::log("Block Difficulty: " + zTxPoW.getBlockDifficulty().to0xString());
        // MinimaLogger::log("Match: " + std::string(zTxPoW.getBlockDifficulty().isEqual(blockdifficulty) ? "YES" : "NO"));
        // DEBUG END
        
        if (!zTxPoW.getBlockDifficulty().isEqual(blockdifficulty)) {
            MinimaLogger::log("Incorrect TxPoW block difficulty @ " + zTxPoW.getBlockNumber().toString() + " " + zTxPoW.getTxPoWID());
            return false;
        }

        // Check Magic numbers
        const org::minima::objects::Magic& txpowmagic = zTxPoW.getMagic();
        if (!txpowmagic.checkSame(zParentNode->getTxPoW().getMagic().calculateNewCurrent())) {
            MinimaLogger::log("Incorrect Magic values " + std::to_string(zTxPoW.getBlockTransactions().size()) + " " + zTxPoW.getTxPoWID());
            return false;
        }

        // Check Num Transactions
        if (zTxPoW.getBlockTransactions().size() > (std::size_t)txpowmagic.getMaxNumTxns().getAsInt()) {
            MinimaLogger::log("Too many transactions in block " + std::to_string(zTxPoW.getBlockTransactions().size()) + " " + zTxPoW.getTxPoWID());
            return false;
        }

        // Unique CoinIDs across all txs
        std::vector<std::string> allcoinid;
        if (zTxPoW.isTransaction()) {
            auto& proofs_main = zTxPoW.getWitness().getAllCoinProofs();
            for (auto& up : proofs_main) {
                if (up) allcoinid.push_back(up->getCoin().getCoinID().to0xString());
            }
            auto& proofs_burn = zTxPoW.getBurnWitness().getAllCoinProofs();
            for (auto& up : proofs_burn) {
                if (up) allcoinid.push_back(up->getCoin().getCoinID().to0xString());
            }
        }
        for (auto* txp : zTransactions) {
            if (!txp) continue;
            if (txp->isTransaction()) {
                auto& proofs_main = txp->getWitness().getAllCoinProofs();
                for (auto& up : proofs_main) {
                    if (up) allcoinid.push_back(up->getCoin().getCoinID().to0xString());
                }
                auto& proofs_burn = txp->getBurnWitness().getAllCoinProofs();
                for (auto& up : proofs_burn) {
                    if (up) allcoinid.push_back(up->getCoin().getCoinID().to0xString());
                }
            }
        }
        std::unordered_set<std::string> coinset(allcoinid.begin(), allcoinid.end());
        if (coinset.size() != allcoinid.size()) {
            MinimaLogger::log("Invalid TxPoW Block with non unique CoinIDs " + zTxPoW.getTxPoWID());
            return false;
        }

        // Parent MMR
        MMR& parentMMR = zParentNode->getMMR();

        // Check this TxPoW (if it is a transaction)
        if (zTxPoW.isTransaction()) {
            // FIX: Pass zTxPoW as non-const
            bool valid = checkTxPoWSimple(parentMMR, zTxPoW, zTxPoW, true);
            if (!valid) return false;
        }

        // Check internal transactions
        for (auto* txp : zTransactions) {
            if (!txp) continue;
            // H17: txp is already non-const
            bool valid = checkTxPoWSimple(parentMMR, *txp, zTxPoW, true);
            if (!valid) return false;
        }

        // Construct MMR to verify root/total
        auto txpow_copy_up = zTxPoW.deepCopy();
        std::vector<org::minima::objects::TxPoW*> nonConstTransactions;
        nonConstTransactions.reserve(zTransactions.size());

        for (auto* constTxPoW : zTransactions) {
            nonConstTransactions.push_back(constTxPoW);
        }

        // Now, call the constructor with the new non-const vector
        TxBlock txblock(parentMMR, std::move(*txpow_copy_up), nonConstTransactions);
        TxPoWTreeNode node(txblock, false); // This calculates the MMR

        auto root = node.getMMR().getRoot();
        if (!root) {
            MinimaLogger::log("ERROR : Could not calculate MMR root! @ " + zTxPoW.getBlockNumber().toString() + " " + zTxPoW.getTxPoWID());
            return false;
        }
        if (!root->getData().isEqual(zTxPoW.getMMRRoot()) || !root->getValue().isEqual(zTxPoW.getMMRTotal())) {
            MinimaLogger::log("ERROR : MMR in TxPOW block and calculated don't match! @ " + zTxPoW.getBlockNumber().toString() + " " + zTxPoW.getTxPoWID());
            return false;
        }

    } catch (const std::exception& exc) {
        MinimaLogger::log("ERROR checking TxPoW Block..");
        MinimaLogger::log(exc);
        return false;
    } catch (...) {
        MinimaLogger::log("ERROR checking TxPoW Block.. unknown exception");
        return false;
    }

    return true;
}

// zTxBlock is NON-CONST
bool TxPoWChecker::checkTxBlockOnly(TxPoWTreeNode* zParentNode, TxBlock& zTxBlock) {
    if (!zParentNode) {
        MinimaLogger::log("checkTxBlockOnly with NULL parent node");
        return false;
    }

    try {
        // zTxBlock is non-const, so getTxPoW() returns non-const TxPoW&
        const TxPoW& txpow = zTxBlock.getTxPoW();

        // ChainID
        if (!txpow.getChainID().isEqual(TxPoWChecker::CURRENT_NETWORK)) {
            MinimaLogger::log("Invalid Block ChainID! " + txpow.getChainID().to0xString() + " " + txpow.getTxPoWID());
            return false;
        }

        // Block number
        if (!txpow.getBlockNumber().isEqual(zParentNode->getBlockNumber().increment())) {
            MinimaLogger::log("Invalid TxPoW block with wrong blocknumber " + txpow.getTxPoWID());
            return false;
        }

        // Parents
        if (!checkParents(*zParentNode, txpow)) {
            MinimaLogger::log("Invalid TxPoW Super Parents " + txpow.getTxPoWID());
            return false;
        }

        // Time window vs median
        std::shared_ptr<TxPoWTreeNode> median = org::minima::system::brains::TxPoWGenerator::getMedianTimeBlock(
            zParentNode->shared_from_this(),
            org::minima::system::params::GlobalParams::MEDIAN_BLOCK_CALC * 2);
        if (!median) {
            MinimaLogger::log("Failed to get median time block");
            return false;
        }
        MiniNumber maxtime = median->getTxPoW().getTimeMilli().add(MAX_TIME_FUTURE);
        if (txpow.getTimeMilli().isLess(median->getTxPoW().getTimeMilli())) {
            MinimaLogger::log("Invalid TxPoW TimeMilli less than median 1 hr back " + txpow.getTxPoWID());
            return false;
        } else if (txpow.getTimeMilli().isMore(maxtime)) {
            MinimaLogger::log("Invalid TxPoW TimeMilli more than 24 hrs in future " + txpow.getTxPoWID());
            return false;
        }

        // Block difficulty
        MiniData blockdifficulty = org::minima::system::brains::TxPoWGenerator::getBlockDifficulty(zParentNode->shared_from_this());
        if (!txpow.getBlockDifficulty().isEqual(blockdifficulty)) {
            MinimaLogger::log("Incorrect TxPoW block difficulty @ " + txpow.getBlockNumber().toString() + " " + txpow.getTxPoWID());
            return false;
        }

        // Magic check
        const org::minima::objects::Magic& txpowmagic = txpow.getMagic();
        if (!txpowmagic.checkSame(zParentNode->getTxPoW().getMagic().calculateNewCurrent())) {
            MinimaLogger::log("Incorrect Magic values " + std::to_string(txpow.getBlockTransactions().size()) + " " + txpow.getTxPoWID());
            return false;
        }

    } catch (const std::exception& exc) {
        MinimaLogger::log("ERROR checking TxBlock in slave node..");
        MinimaLogger::log(exc);
        return false;
    } catch (...) {
        MinimaLogger::log("ERROR checking TxBlock in slave node.. unknown exception");
        return false;
    }

    return true;
}

// zTxPoW is NON-CONST
bool TxPoWChecker::checkTxPoWBasic(TxPoW& zTxPoW) {
    // ChainID
    if (!zTxPoW.getChainID().isEqual(CURRENT_NETWORK)) {
        MinimaLogger::log("Wrong TxPoW ChainID! " + zTxPoW.getChainID().to0xString() + " " + zTxPoW.getTxPoWID());
        return false;
    }

    // Unique coins in main/burn
    std::vector<std::string> allcoinid;
    if (zTxPoW.isTransaction()) {
        auto& proofs_main = zTxPoW.getWitness().getAllCoinProofs();
        for (auto& up : proofs_main) {
            if (up) allcoinid.push_back(up->getCoin().getCoinID().to0xString());
        }
        auto& proofs_burn = zTxPoW.getBurnWitness().getAllCoinProofs();
        for (auto& up : proofs_burn) {
            if (up) allcoinid.push_back(up->getCoin().getCoinID().to0xString());
        }
    }
    std::unordered_set<std::string> coinset(allcoinid.begin(), allcoinid.end());
    if (coinset.size() != allcoinid.size()) {
        MinimaLogger::log("Invalid TxPoW Transaction / Burn with non unique CoinIDs " + zTxPoW.getTxPoWID() +
                          " uniquesize:" + std::to_string(coinset.size()) + " txncoins:" + std::to_string(allcoinid.size()));
        return false;
    }

    // Main transaction
    if (!checkTxPoWBasic_impl(zTxPoW.getTxPoWID(), zTxPoW.getTransaction(), zTxPoW.getWitness())) {
        return false;
    }

    // Link hash of main must be ZERO
    if (!zTxPoW.getTransaction().getLinkHash().isEqual(MiniData::ZERO_TXPOWID())) {
        MinimaLogger::log("Invalid LinkHash for Transaction ( NOT 0x00 ) " + zTxPoW.getTxPoWID());
        return false;
    }

    // Burn tx if not empty
    if (!zTxPoW.getBurnTransaction().isEmpty()) {
        if (!zTxPoW.getBurnTransaction().getLinkHash().isEqual(zTxPoW.getTransaction().getTransactionID())) {
            MinimaLogger::log("Invalid LinkHash for Burn Transaction " + zTxPoW.getTxPoWID());
            return false;
        }
        return checkTxPoWBasic_impl(zTxPoW.getTxPoWID(), zTxPoW.getBurnTransaction(), zTxPoW.getBurnWitness());
    }

    return true;
}

// zTransaction and zWitness are NON-CONST
bool TxPoWChecker::checkTxPoWBasic_impl(const std::string& zTxPoWID,
                                        Transaction& zTransaction,
                                        Witness& zWitness) {
    if (zTransaction.isEmpty()) {
        return true;
    }

    if (!zTransaction.checkValid()) {
        MinimaLogger::log("Invalid Transaction Inputs and Outputs.. " + zTransaction.toJSON().toString());
        return false;
    }

    auto& inputs = zTransaction.getAllInputs();
    int ins = static_cast<int>(inputs.size());
    if (ins == 0) {
        MinimaLogger::log("Transaction MUST have at least 1 input @ " + zTxPoWID);
        return false;
    }

    auto& mmrproofs = zWitness.getAllCoinProofs();
    if (ins != static_cast<int>(mmrproofs.size())) {
        MinimaLogger::log("Wrong Number of MMR Proofs Inputs:" + std::to_string(ins) +
                          " MMRProofs:" + std::to_string(mmrproofs.size()) + " @ " + zTxPoWID);
        return false;
    }

    std::unordered_set<std::string> allcoinsused;

    for (int i = 0; i < ins; ++i) {
        Coin& input = *inputs[i];
        CoinProof& cproof = *mmrproofs[i];

        std::string coinid = cproof.getCoin().getCoinID().to0xString();
        if (allcoinsused.find(coinid) != allcoinsused.end()) {
            MinimaLogger::log("CoinID used more than once @ " + std::to_string(i) + " in " + zTxPoWID);
            return false;
        }
        allcoinsused.insert(coinid);

        bool amount  = input.getAmount().isEqual(cproof.getCoin().getAmount());
        bool address = input.getAddress().isEqual(cproof.getCoin().getAddress());
        bool token   = input.getTokenID().isEqual(cproof.getCoin().getTokenID());
        if (!amount || !address || !token) {
            MinimaLogger::log("Input coin details don't match coinproof " + zTxPoWID);
            return false;
        }

        if (!input.getCoinID().isEqual(Coin::COINID_ELTOO)) {
            if (!input.getCoinID().isEqual(cproof.getCoin().getCoinID())) {
                MinimaLogger::log("CoinID input " + std::to_string(i) + " doesn't match proof " + zTxPoWID);
                return false;
            }
        }

        if (!input.getTokenID().isEqual(Token::TOKENID_MINIMA)) {
            const Token* intok = input.getToken();
            if (intok == nullptr) {
                MinimaLogger::log("Null token for Coin input " + std::to_string(i) + " " + zTxPoWID);
                return false;
            }
            if (!input.getTokenID().isEqual(*intok->getTokenID())) {
                MinimaLogger::log("TokenID in Coin input " + std::to_string(i) + " doesn't match token " + zTxPoWID);
                return false;
            }
            const Token* cptok = cproof.getCoin().getToken();
            if (cptok == nullptr) {
                MinimaLogger::log("Null token for MMR Proof input " + std::to_string(i) + " " + zTxPoWID);
                return false;
            }
            if (!cproof.getCoin().getTokenID().isEqual(*cptok->getTokenID())) {
                MinimaLogger::log("TokenID in MMR Proof input " + std::to_string(i) + " doesn't match token " + zTxPoWID);
                return false;
            }
        }

        if (cproof.getCoin().getSpent()) {
            MinimaLogger::log("Trying to spend spent coin..");
            return false;
        }

        // FIX: getScript is const, returns const*, assign to const*
        const ScriptProof* prfs = zWitness.getScript(input.getAddress());
        if (prfs == nullptr) {
            MinimaLogger::log("Script Missing from TxPoW for address " + input.getAddress().to0xString());
            return false;
        }
    }

    auto& outputs = zTransaction.getAllOutputs();
    for (auto& upc : outputs) {
        Coin& cc = *upc;
        if (!cc.getTokenID().isEqual(Token::TOKENID_MINIMA) && !cc.getTokenID().isEqual(Token::TOKENID_CREATE)) {
            if (cc.getToken() == nullptr) {
                MinimaLogger::log("Incorrect output token with NULL token..");
                return false;
            } else if (!cc.getToken()->getTokenID()->isEqual(cc.getTokenID())) {
                MinimaLogger::log("Incorrect output token with different tokenid..");
                return false;
            }
        } else if (cc.getTokenID().isEqual(Token::TOKENID_CREATE)) {
            if (cc.getToken() == nullptr) {
                MinimaLogger::log("Incorrect output token for create token with NULL token..");
                return false;
            }
        }
    }

    return true;
}

// FIX: zTxPoW and zBlock are NON-CONST
bool TxPoWChecker::checkTxPoWSimple(MMR& zTipMMR,
                                    TxPoW& zTxPoW, // NOT CONST
                                    TxPoW& zBlock, // NOT CONST
                                    bool zLog) {
    if (zTxPoW.getTxnDifficulty().isMore(zBlock.getMagic().getMinTxPowWork())) {
        MinimaLogger::log("TxPoW difficulty too low.. " + zTxPoW.getTxPoWID());
        return false;
    }

    long long maxsize = zBlock.getMagic().getMaxTxPoWSize().getAsLong();
    long long size    = zTxPoW.getSizeinBytesWithoutBlockTxns();
    if (size > maxsize) {
        MinimaLogger::log("TxPoW size too large.. " + std::to_string(size) + " " + zTxPoW.getTxPoWID());
        return false;
    }

    // FIX: checkMaxStateStoreSize takes a non-const TxPoW&
    if (!org::minima::system::network::minima::RelayPolicy::checkMaxStateStoreSize(zTxPoW, maxsize)) {
        MinimaLogger::log("TxPoW state store too large..");
        return false;
    }

    if (!checkMMR(zTipMMR, zTxPoW, zLog)) {
        return false;
    }

    return checkTxPoWScripts(zTipMMR, zTxPoW, zBlock);
}

// FIX: zTxPoW and zBlock are NON-CONST
bool TxPoWChecker::checkTxPoWScripts(MMR& zTipMMR, TxPoW& zTxPoW, TxPoW& zBlock) {
    if (!checkTxPoWScripts_impl(zTipMMR, zTxPoW.getTransaction(), zTxPoW.getWitness(), zBlock)) {
        return false;
    }
    return checkTxPoWScripts_impl(zTipMMR, zTxPoW.getBurnTransaction(), zTxPoW.getBurnWitness(), zBlock);
}

// FIX: All parameters are NON-CONST (except zTipMMR)
bool TxPoWChecker::checkTxPoWScripts_impl(MMR& /*zTipMMR*/,
                                          Transaction& zTransaction, // NOT CONST
                                          Witness& zWitness,     // NOT CONST
                                          TxPoW& zBlock) {     // NOT CONST
    if (zTransaction.isCheckedMonotonic()) {
        return zTransaction.mIsValid;
    }

    zTransaction.mHaveCheckedMonotonic = true;
    zTransaction.mIsMonotonic = true;
    zTransaction.mIsValid = false;

    if (zTransaction.isEmpty()) {
        zTransaction.mIsValid = true;
        return true;
    }

    int maxops = zBlock.getMagic().getMaxKISSOps().getAsInt();

    auto& mmrproofs = zWitness.getAllCoinProofs();
    int ins = static_cast<int>(mmrproofs.size());
    int inputssize = static_cast<int>(zTransaction.getAllInputs().size());
    if (ins != inputssize) {
        MinimaLogger::log("Wrong number of MMRProofs(" + std::to_string(ins) + ") for Inputs(" + std::to_string(inputssize) + ")");
        return false;
    }

    for (int i = 0; i < ins; ++i) {
        CoinProof& cproof = *mmrproofs[i];

        // FIX: getScript is const, returns const*, assign to const*
        const ScriptProof* prfs = zWitness.getScript(cproof.getCoin().getAddress());
        if (prfs == nullptr) {
            MinimaLogger::log("Script NOT found for address : " + cproof.getCoin().getAddress().to0xString());
            return false;
        }

        std::string script = prfs->getScript().toString();

        std::vector<std::unique_ptr<org::minima::objects::StateVariable>> prevStateForAddr;
        {
            auto coinCopy = cproof.getCoin().deepCopy();
            prevStateForAddr = std::move(coinCopy->getState());
        }

        org::minima::kissvm::Contract contract(script,
                                               zWitness.getAllSignatureKeys(),
                                               zWitness, // Pass non-const
                                               zTransaction, // Pass non-const
                                               std::move(prevStateForAddr));
        contract.setMaxInstructions(maxops);
        contract.setGlobals(zBlock.getBlockNumber(),
                            zBlock.getTimeMilli(),
                            zTransaction, // Pass non-const
                            i,
                            cproof.getCoin().getBlockCreated(),
                            script);
        contract.run();

        if (!contract.isMonotonic()) {
            zTransaction.mIsMonotonic = false;
        }

        if (!contract.isSuccess()) {
            if (org::minima::system::params::GeneralParams::SCRIPTLOGS) {
                MinimaLogger::log("Script FAIL input:" + std::to_string(i) + " " + contract.getCompleteTraceLog());
            } else {
                MinimaLogger::log("Script FAIL input:" + std::to_string(i) + " " + script);
            }
            return false;
        }

        if (!cproof.getCoin().getTokenID().isEqual(Token::TOKENID_MINIMA)) {
            const Token* tok = cproof.getCoin().getToken();
            if (!tok) {
                MinimaLogger::log("Missing token on non-MINIMA coin");
                return false;
            }
            std::string tokscript = tok->getTokenScript().toString();
            auto start = tokscript.find_first_not_of(" \t\r\n");
            auto end   = tokscript.find_last_not_of(" \t\r\n");
            std::string tokscript_trim = (start == std::string::npos) ? "" : tokscript.substr(start, end - start + 1);

            if (tokscript_trim != "RETURN TRUE") {
                std::vector<std::unique_ptr<org::minima::objects::StateVariable>> prevStateForTok;
                {
                    auto coinCopy2 = cproof.getCoin().deepCopy();
                    prevStateForTok = std::move(coinCopy2->getState());
                }

                org::minima::kissvm::Contract tokcontract(tokscript_trim,
                                                           zWitness.getAllSignatureKeys(),
                                                           zWitness, // Pass non-const
                                                           zTransaction, // Pass non-const
                                                           std::move(prevStateForTok));
                tokcontract.setMaxInstructions(maxops);
                tokcontract.setGlobals(zBlock.getBlockNumber(),
                                       zBlock.getTimeMilli(),
                                       zTransaction, // Pass non-const
                                       i,
                                       cproof.getCoin().getBlockCreated(),
                                       tokscript_trim);
                tokcontract.run();

                if (!tokcontract.isMonotonic()) {
                    zTransaction.mIsMonotonic = false;
                }

                if (!tokcontract.isSuccess()) {
                    if (org::minima::system::params::GeneralParams::SCRIPTLOGS) {
                        MinimaLogger::log("Token Script FAIL input:" + std::to_string(i) + " " + tokcontract.getCompleteTraceLog());
                    } else {
                        MinimaLogger::log("Token Script FAIL input:" + std::to_string(i) + " " + tokscript_trim);
                    }
                    return false;
                }
            }
        }
    }

    zTransaction.mIsValid = true;
    return true;
}

// Overload 1
bool TxPoWChecker::checkMMR(MMR& zTipMMR, TxPoW& zTxPoW) {
    return checkMMR(zTipMMR, zTxPoW, true);
}

// Overload 2: FIX: zTxPoW is NON-CONST
bool TxPoWChecker::checkMMR(MMR& zTipMMR, TxPoW& zTxPoW, bool zLog) {
    if (!checkMMR_impl(zTipMMR, zTxPoW.getWitness(), zLog)) {
        return false;
    }
    return checkMMR_impl(zTipMMR, zTxPoW.getBurnWitness(), zLog);
}

// FIX: zWitness is NON-CONST
bool TxPoWChecker::checkMMR_impl(MMR& zTipMMR, Witness& zWitness, bool zLog) {
    auto& mmrproofs = zWitness.getAllCoinProofs();
    int proofs = static_cast<int>(mmrproofs.size());

    for (int i = 0; i < proofs; ++i) {
        CoinProof& cproof = *mmrproofs[i];
        Coin& txcoin = cproof.getCoin();

        auto mmrcoin = MMRData::CreateMMRDataLeafNode(txcoin, txcoin.getAmount());

        bool validmmr = zTipMMR.checkProofTimeValid(
            cproof.getCoin().getMMREntryNumber(),
            *mmrcoin,
            cproof.getMMRProof());

        if (!validmmr) {
            if (zLog) {
                MinimaLogger::log("Invalid MMR Proof! @ " + zTipMMR.getBlockTime().toString());
            }
            return false;
        }
    }
    return true;
}

// zTxPoW is NON-CONST
bool TxPoWChecker::checkSignatures(TxPoW& zTxPoW) {
    bool valid = true;
    auto transid = zTxPoW.getTransaction().getTransactionID();

    auto& allsigs = zTxPoW.getWitness().getAllSignatures();
    for (auto& upsig : allsigs) {
        const org::minima::objects::keys::Signature& sig = *upsig;

        org::minima::objects::keys::TreeKey tk;
        tk.setPublicKey(sig.getRootPublicKey());

        if (!tk.verify(transid, sig)) {
            MinimaLogger::log("SIGNATURE FAIL : " + zTxPoW.getTxPoWID());
            valid = false;
            break;
        }
    }

    if (!valid) return false;

    // Burn transaction signatures
    auto& burn_sigs = zTxPoW.getBurnWitness().getAllSignatures();
    auto burn_transid = zTxPoW.getBurnTransaction().getTransactionID();
    for (auto& upsig : burn_sigs) {
        const org::minima::objects::keys::Signature& sig = *upsig;

        org::minima::objects::keys::TreeKey tk;
        tk.setPublicKey(sig.getRootPublicKey());

        if (!tk.verify(burn_transid, sig)) {
            MinimaLogger::log("SIGNATURE FAIL (burn) : " + zTxPoW.getTxPoWID());
            return false;
        }
    }

    return true;
}

// zTxPoW is NON-CONST
bool TxPoWChecker::checkMemPoolCoins(TxPoW& zTxPoW) {
    org::minima::database::txpowdb::TxPoWDB& txpdb = org::minima::database::MinimaDB::getDB()->getTxPoWDB();

    auto& proofs_main = zTxPoW.getWitness().getAllCoinProofs();
    for (auto& cp : proofs_main) {
        if (txpdb.checkMempoolCoins(cp->getCoin().getCoinID())) {
            return true;
        }
    }

    auto& proofs_burn = zTxPoW.getBurnWitness().getAllCoinProofs();
    for (auto& cp : proofs_burn) {
        if (txpdb.checkMempoolCoins(cp->getCoin().getCoinID())) {
            return true;
        }
    }

    return false;
}

// zBlock is CONST (this function only reads)

bool TxPoWChecker::checkParents(TxPoWTreeNode& zTip, const TxPoW& zBlock) {
    // MinimaLogger::log("=== DEBUG checkParents START ===");
    // MinimaLogger::log("Validating block " + zBlock.getBlockNumber().toString() + 
    //                   " TxPoWID=" + zBlock.getTxPoWID());
    
    // Log all super parents of the block being validated
    // MinimaLogger::log("Block's super parents:");
    // for (int i = 0; i < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS; i++) {
    //     MiniData sp = zBlock.getSuperParent(i);
    //     MinimaLogger::log("  Level " + std::to_string(i) + ": " + sp.to0xString());
    // }
    
    int blocksup = 0;
    TxPoWTreeNode* current = &zTip;
    
    // MinimaLogger::log("Walking backward through TxPoWTree from tip " + 
    //                   zTip.getTxPoW().getBlockNumber().toString());
    
    while (current != nullptr) {
        TxPoW& txpow = current->getTxPoW();
        MiniData txdata = txpow.getTxPoWIDData();
        int superlevel = txpow.getSuperLevel();
        
        // MinimaLogger::log("Tree node: Block " + txpow.getBlockNumber().toString() + 
        //                   " TxPoWID=" + txpow.getTxPoWID() + 
        //                   " SuperLevel=" + std::to_string(superlevel));

        while (superlevel >= blocksup) {
            MiniData superparent = zBlock.getSuperParent(blocksup);
            
            // MinimaLogger::log("  Checking: blocksup=" + std::to_string(blocksup) + 
            //                   " expected=" + superparent.to0xString() + 
            //                   " actual=" + txdata.to0xString() + 
            //                   " match=" + (superparent.isEqual(txdata) ? "YES" : "NO"));
            
            if (!superparent.isEqual(txdata)) {
                // MinimaLogger::log("=== FAILED: Super parent mismatch in tree ===");
                return false;
            }
            blocksup++;
        }

        std::shared_ptr<TxPoWTreeNode> parent = current->getParent();
        current = parent ? parent.get() : nullptr;
    }
    
    // MinimaLogger::log("Finished tree validation. Now checking cascade. blocksup=" + std::to_string(blocksup));

    auto cnode = org::minima::database::MinimaDB::getDB()->getCascade().getTip();
    
    // if (cnode != nullptr) {
    //     MinimaLogger::log("Cascade tip: Block " + cnode->getTxPoW().getBlockNumber().toString() + 
    //                       " TxPoWID=" + cnode->getTxPoW().getTxPoWID());
    // } else {
    //     MinimaLogger::log("Cascade is empty");
    // }
    
    while (cnode != nullptr) {
        TxPoW& txpow = cnode->getTxPoW();
        MiniData txdata = txpow.getTxPoWIDData();
        int superlevel = txpow.getSuperLevel();
        
        // MinimaLogger::log("Cascade node: Block " + txpow.getBlockNumber().toString() + 
        //                   " TxPoWID=" + txpow.getTxPoWID() + 
        //                   " SuperLevel=" + std::to_string(superlevel) + 
        //                   " Level=" + std::to_string(cnode->getLevel()));

        while (superlevel >= blocksup) {
            MiniData superparent = zBlock.getSuperParent(blocksup);
            
            // MinimaLogger::log("  Checking: blocksup=" + std::to_string(blocksup) + 
            //                   " expected=" + superparent.to0xString() + 
            //                   " actual=" + txdata.to0xString() + 
            //                   " match=" + (superparent.isEqual(txdata) ? "YES" : "NO"));
            
            if (!superparent.isEqual(txdata)) {
                // MinimaLogger::log("=== FAILED: Super parent mismatch in cascade ===");
                // MinimaLogger::log("Expected super parent at level " + std::to_string(blocksup) + 
                //                   " to be " + superparent.to0xString());
                // MinimaLogger::log("But cascade node has TxPoWID " + txdata.to0xString());
                // MinimaLogger::log("Cascade node super level is " + std::to_string(superlevel));
                // MinimaLogger::log("Cascade node cascade level is " + std::to_string(cnode->getLevel()));
                return false;
            }
            blocksup++;
        }

        cnode = org::minima::database::MinimaDB::getDB()->getCascade().getParent(cnode);
    }
    
    // MinimaLogger::log("Finished cascade validation. Checking remaining super parents. blocksup=" + 
    //                   std::to_string(blocksup));

    for (int i = blocksup; i < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS; i++) {
        MiniData sp = zBlock.getSuperParent(i);
        // MinimaLogger::log("  Level " + std::to_string(i) + ": " + sp.to0xString() + 
        //                   " (should be 0x00)");
        
        if (!sp.isEqual(MiniData::ZERO_TXPOWID())) {
            // MinimaLogger::log("=== FAILED: Non-zero super parent at level " + std::to_string(i) + 
            //                   " after chain ended ===");
            return false;
        }
    }

    // MinimaLogger::log("=== DEBUG checkParents SUCCESS ===");
    return true;
}

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org