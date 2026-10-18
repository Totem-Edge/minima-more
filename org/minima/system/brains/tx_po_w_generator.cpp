#include "org/minima/system/brains/tx_po_w_generator.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <unordered_set>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"       // Ensure TxPowTree is complete
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/magic.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"                     // Ensure MMR is complete
#include "org/minima/objects/mmr/m_m_r_data.hpp"

#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

using boost::multiprecision::cpp_int;
using boost::multiprecision::cpp_dec_float_100;

// Forward declarations for TxPoWChecker to avoid missing-header dependency
namespace org { namespace minima { namespace objects { namespace mmr { class MMR; } } } }
namespace org { namespace minima { namespace system { namespace brains {
class TxPoWChecker {
public:
    static const org::minima::objects::base::MiniNumber MAX_TIME_FUTURE;
    static bool checkTxPoWSimple(org::minima::objects::mmr::MMR& zMMR,
                                 org::minima::objects::TxPoW& zMempoolTxPoW,
                                 org::minima::objects::TxPoW& zCurrentTxPoW,
                                 bool zFastCheck);
};
} } } }

namespace {

// Convert boost::multiprecision::cpp_int to MiniData.
org::minima::objects::base::MiniData cpp_int_to_mini_data(const cpp_int& value)
{
    using org::minima::objects::base::MiniData;

    if (value == 0) {
        std::vector<std::uint8_t> zero(1, 0);
        return MiniData(zero);
    }

    cpp_int v = value;
    std::vector<std::uint8_t> bytes;
    
    // Extract bytes (Little Endian extraction)
    while (v > 0) {
        std::uint8_t b = static_cast<std::uint8_t>(v & 0xff);
        bytes.push_back(b);
        v >>= 8;
    }
    
    // Convert to Big Endian (Java style)
    std::reverse(bytes.begin(), bytes.end());

    // Don't add 0x00 for positive numbers with high bit set
    // Java's BigInteger.toByteArray() adds it, then MiniData removes it
    // We skip both steps and use the minimal representation directly
    return MiniData(bytes);
}

// Parse decimal string to cpp_int
cpp_int cpp_int_from_dec(const std::string& dec)
{
    std::istringstream iss(dec);
    cpp_int out;
    iss >> out;
    return out;
}

// Average difficulty over N blocks (Java BigInteger average)
cpp_int getAverageDifficulty_impl(
    const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zTopBlock,
    const org::minima::objects::base::MiniNumber& zBlocksBack)
{
    using org::minima::database::txpowtree::TxPoWTreeNode;
    using org::minima::objects::base::MiniData;

    cpp_int total = 0;
    int totalblock = zBlocksBack.getAsInt();

    std::shared_ptr<TxPoWTreeNode> current = zTopBlock;
    int counter = 0;

    while (counter < totalblock) {
        MiniData difficulty = current->getTxPoW().getBlockDifficulty();
        cpp_int diffval = cpp_int_from_dec(difficulty.getDataValue());
        total += diffval;

        current = current->getParent();
        ++counter;
    }

    if (counter == 0) {
        return 0;
    }

    cpp_int avg = total / cpp_int(counter);
    return avg;
}


} // anonymous namespace

namespace org {
namespace minima {
namespace system {
namespace brains {

// Static members
bool TxPoWGenerator::MEMPOOL_FULL = false;
org::minima::objects::base::MiniNumber TxPoWGenerator::MIN_MEMPOOL_BURN = org::minima::objects::base::MiniNumber::ZERO();

// Difficulty bounds
const org::minima::objects::base::MiniNumber TxPoWGenerator::MAX_SPBOUND_DIFFICULTY = org::minima::objects::base::MiniNumber(std::string("2.0"));
const org::minima::objects::base::MiniNumber TxPoWGenerator::MIN_SPBOUND_DIFFICULTY = org::minima::objects::base::MiniNumber(std::string("0.5"));

bool TxPoWGenerator::isMempoolFull() {
    return MEMPOOL_FULL;
}

org::minima::objects::base::MiniNumber TxPoWGenerator::getMinMempoolBurn() {
    return MIN_MEMPOOL_BURN;
}

org::minima::objects::base::MiniData
TxPoWGenerator::calculateDifficultyData(const org::minima::objects::base::MiniNumber& zHashes) {
    // MAX_VALDEC() is decimal string of (2^256 - 1)
    cpp_int maxval = cpp_int_from_dec(org::minima::utils::Crypto::MAX_VALDEC());
    cpp_int denom = zHashes.getAsBigInteger();
    if (denom == 0) {
        throw std::runtime_error("Division by zero in calculateDifficultyData");
    }
    cpp_int res = maxval / denom;
    return cpp_int_to_mini_data(res);
}

std::unique_ptr<org::minima::objects::TxPoW>
TxPoWGenerator::generateTxPoW(const org::minima::objects::Transaction& zTransaction,
                              const org::minima::objects::Witness& zWitness) {
    return generateTxPoW(zTransaction, zWitness, nullptr, nullptr);
}

std::unique_ptr<org::minima::objects::TxPoW>
TxPoWGenerator::generateTxPoW(const org::minima::objects::Transaction& zTransaction,
                              const org::minima::objects::Witness& zWitness,
                              const org::minima::objects::Transaction* zBurnTransaction,
                              const org::minima::objects::Witness* zBurnWitness) {
    using org::minima::database::MinimaDB;
    using org::minima::database::txpowtree::TxPoWTreeNode;
    using org::minima::objects::TxPoW;
    using org::minima::objects::TxBlock;
    using org::minima::objects::Witness;
    using org::minima::objects::Magic;
    using org::minima::objects::base::MiniNumber;
    using org::minima::objects::base::MiniData;

    // Base TxPoW
    TxPoW txpow;
    // try {
    //     MiniNumber blockNum = txpow.getBlockNumber();
    //     org::minima::utils::MinimaLogger::log("DEBUG generateTxPoW: Created base txpow, block=" + blockNum.toString());
    // } catch (...) {
    //     org::minima::utils::MinimaLogger::log("DEBUG generateTxPoW: Created base txpow - ERROR accessing block number!");
    // }

    //
    // FIX 1: Use dot (.) operator on the reference returned by getTxPoWTree()
    //
    auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

    // Set the block number (tip + 1)
    txpow.setBlockNumber(tip->getTxPoW().getBlockNumber().increment());

    // Current time in millis
    std::int64_t nowms = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    MiniNumber millitime(static_cast<long long>(nowms));

    // Check TimeMilli is acceptable using median bounds
    auto mednode = getMedianTimeBlock(tip, org::minima::system::params::GlobalParams::MEDIAN_BLOCK_CALC * 2);
    MiniNumber mintime = mednode->getTxPoW().getTimeMilli();
    MiniNumber maxtime = mintime.add(TxPoWChecker::MAX_TIME_FUTURE);

    if (millitime.isLess(mintime)) {
        millitime = mintime.add(MiniNumber(1000 * 50)); // add 50 seconds
    } else if (millitime.isMore(maxtime)) {
        millitime = maxtime;
    }

    // Set the current time
    txpow.setTimeMilli(millitime);

    // Set the Transaction and Witness
    txpow.setTransaction(zTransaction);
    txpow.setWitness(zWitness);


    // org::minima::utils::MinimaLogger::log("DEBUG_GEN: Original TxID: " + zTransaction.getTransactionID().to0xString());
    // org::minima::utils::MinimaLogger::log("DEBUG_GEN: TxPoW TxID: " + txpow.getTransaction().getTransactionID().to0xString());
    if (zTransaction.getTransactionID().isEqual(txpow.getTransaction().getTransactionID())) {
        // org::minima::utils::MinimaLogger::log("DEBUG_GEN: Transaction IDs MATCH ✓");
    } else {
        // org::minima::utils::MinimaLogger::log("DEBUG_GEN: Transaction IDs MISMATCH ✗ ← BUG!");
    }

    // Optional Burn Transaction / Witness
    if (zBurnTransaction != nullptr) {
        txpow.setBurnTransaction(*zBurnTransaction);
        if (zBurnWitness != nullptr) {
            txpow.setBurnWitness(*zBurnWitness);
        } else {
            txpow.setBurnWitness(Witness());
        }
    }

    //
    // FIX 2: Get UserDB as a reference (auto&) to avoid copy
    //
    auto& udb = MinimaDB::getDB()->getUserDB();

    Magic txpowmagic = tip->getTxPoW().getMagic().calculateNewCurrent();
    //
    // FIX 3: Use dot (.) operator on the 'udb' reference
    //
    txpowmagic.setDesiredKISSVM(udb.getMagicDesiredKISSVM());
    txpowmagic.setDesiredMaxTxPoWSize(udb.getMagicMaxTxPoWSize());
    txpowmagic.setDesiredMaxTxns(udb.getMagicMaxTxns());
    txpow.setMagic(txpowmagic);

    // Set parents
    for (int i = 0; i < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS; ++i) {
        txpow.setSuperParent(i, tip->getTxPoW().getSuperParent(i));
    }

    // And now set the correct SBL given the last block
    int sbl = tip->getTxPoW().getSuperLevel();

    // All levels <= sbl now point to the last block
    MiniData tiptxid = tip->getTxPoW().getTxPoWIDData();
    for (int i = sbl; i >= 0; --i) {
        txpow.setSuperParent(i, tiptxid);
    }

    // Block difficulty (bounded)
    MiniData blkdiff = getBlockDifficulty(tip);
    txpow.setBlockDifficulty(blkdiff);

    //
    // FIX 3: Use dot (.) operator on the 'udb' reference
    //
    MiniNumber userhashrate = MinimaDB::getDB()->getUserDB().getHashRate();
    MiniData minhash = calculateDifficultyData(userhashrate);

    // Ensure not less than block difficulty (only at genesis)
    if (minhash.isLess(blkdiff)) {
        minhash = blkdiff;
    }

    // Check acceptable vs. Magic min work; if not, add 10% slack (floor(minwork / 1.1))
    if (minhash.isMore(txpowmagic.getMinTxPowWork())) {
        cpp_int minwork = cpp_int_from_dec(txpowmagic.getMinTxPowWork().getDataValue());
        cpp_int adj = (minwork * 10) / 11;
        minhash = cpp_int_to_mini_data(adj);
    }
    txpow.setTxDifficulty(minhash);

    //
    // FIX 3: Use dot (.) operator on the reference returned by getTxPoWDB()
    //
    auto mempool = MinimaDB::getDB()->getTxPoWDB().getAllUnusedTxns();

    // Sort by burn descending
    std::sort(mempool.begin(), mempool.end(),
        [](const std::shared_ptr<TxPoW>& a, const std::shared_ptr<TxPoW>& b) {
            // higher burn first
            return b->getBurn().compareTo(a->getBurn()) < 0;
        });

    // MAX mempool size (fixed 5000 here per Java snippet)
    int max = 5000;

    int counter = 0;
    std::vector<std::shared_ptr<TxPoW>> newmempool;
    //
    // FIX 2: Get TxPoWDB as a reference (auto&) to avoid copy
    //
    auto& txpdb = MinimaDB::getDB()->getTxPoWDB();
    MEMPOOL_FULL = false;
    for (const auto& memtxp : mempool) {
        if (counter < max) {
            newmempool.push_back(memtxp);
        } else {
            org::minima::utils::MinimaLogger::log(
                std::string("MEMPOOL MAX SIZE REACHED : REMOVED id:") + memtxp->getTxPoWID() +
                " burn:" + memtxp->getBurn().toString());
            //
            // FIX 3: Use dot (.) operator on the 'txpdb' reference
            //
            txpdb.removeMemPoolTxPoW(memtxp->getTxPoWID());

            if (!MEMPOOL_FULL) {
                MEMPOOL_FULL = true;
                MIN_MEMPOOL_BURN = memtxp->getBurn();
            }
        }
        ++counter;
    }
    mempool.swap(newmempool);

    // Final TxPoW transactions put in this block
    std::vector<std::shared_ptr<TxPoW>> chosentxns;

    // Track coins already added (string coinID 0x hex)
    std::unordered_set<std::string> addedcoins;

    // Add coins from main witness
    {
        auto& proofs = txpow.getWitness().getAllCoinProofs();
        for (const auto& proof : proofs) {
            addedcoins.insert(proof->getCoin().getCoinID().to0xString());
        }
    }
    // Add coins from burn witness
    {
        auto& proofs = txpow.getBurnWitness().getAllCoinProofs();
        for (const auto& proof : proofs) {
            addedcoins.insert(proof->getCoin().getCoinID().to0xString());
        }
    }

    // Check all mempool txns
    int totaladded = 0;
    for (const auto& memtxp : mempool) {
        // Is it a transaction
        if (!memtxp->isTransaction()) {
            continue;
        }

        bool valid = true;
        try {
            // How many times checked and rejected
            if (memtxp->getCheckRejectNumber() > 3) {
                valid = false;
            }

            if (valid) {
                // Check if any input coin already added
                bool found = false;
                auto& inputs_tx = memtxp->getWitness().getAllCoinProofs();
                for (const auto& proof : inputs_tx) {
                    if (addedcoins.count(proof->getCoin().getCoinID().to0xString())) {
                        found = true;
                        break;
                    }
                }

                // Check Burn Transaction
                if (!found) {
                    auto& inputs_burn = memtxp->getBurnWitness().getAllCoinProofs();
                    for (const auto& proof : inputs_burn) {
                        if (addedcoins.count(proof->getCoin().getCoinID().to0xString())) {
                            found = true;
                            break;
                        }
                    }
                }

                if (found) {
                    // Already added
                    memtxp->incrementCheckRejectNumber();
                    continue;
                }

                // Check against Magic numbers
                if (memtxp->getSizeinBytesWithoutBlockTxns() > txpowmagic.getMaxTxPoWSize().getAsLong()) {
                    org::minima::utils::MinimaLogger::log(
                        std::string("Mempool txn too big.. ") + memtxp->getTxPoWID() +
                        " size:" + std::to_string(memtxp->getSizeinBytesWithoutBlockTxns()) +
                        " max:" + std::to_string(txpowmagic.getMaxTxPoWSize().getAsLong()));
                    valid = false;
                } else if (memtxp->getTxnDifficulty().isMore(txpowmagic.getMinTxPowWork())) {
                    org::minima::utils::MinimaLogger::log(
                        std::string("Mempool txn TxPoW too low.. ") + memtxp->getTxPoWID());
                    valid = false;
                }
            }

            // Validate against simple checker
            std::shared_ptr<org::minima::objects::mmr::MMR> mmr_ptr(tip, &(tip->getMMR()));
            if (valid && TxPoWChecker::checkTxPoWSimple(*mmr_ptr, const_cast<org::minima::objects::TxPoW&>(*memtxp), txpow, false)) {
                // Add to list
                chosentxns.push_back(memtxp);

                // Add to this TxPoW
                txpow.addBlockTxPOW(memtxp->getTxPoWIDData());

                // Count
                ++totaladded;

                // Add all input coins from transaction
                auto& mem_inputs_tx = memtxp->getWitness().getAllCoinProofs();
                for (const auto& cc : mem_inputs_tx) {
                    addedcoins.insert(cc->getCoin().getCoinID().to0xString());
                }

                // Add all input coins from burn transaction
                auto& mem_inputs_burn = memtxp->getBurnWitness().getAllCoinProofs();
                for (const auto& cc : mem_inputs_burn) {
                    addedcoins.insert(cc->getCoin().getCoinID().to0xString());
                }
            } else {
                // Checked and something wrong..
                memtxp->incrementCheckRejectNumber();
            }
        } catch (const std::exception& exc) {
            org::minima::utils::MinimaLogger::log(
                std::string("ERROR Checking TxPoW ") + memtxp->getTxPoWID() + " " + exc.what());
            valid = false;
        }

        if (!valid) {
            //
            // FIX 3: Use dot (.) operator on the 'txpdb' reference
            //
            MinimaDB::getDB()->getTxPoWDB().removeMemPoolTxPoW(memtxp->getTxPoWID());
        }

        // Max allowed..
        if (totaladded >= txpowmagic.getMaxNumTxns().getAsInt()) {
            break;
        }
    }

    // Calculate the TransactionID - needed for CoinID and MMR
    txpow.calculateTransactionID();

    // Construct the MMR via TxBlock
    // Convert chosen txns to vector<TxPoW*>
    std::vector<TxPoW*> chosen_ptrs;
    chosen_ptrs.reserve(chosentxns.size());
    for (auto& sp : chosentxns) {
        chosen_ptrs.push_back(sp.get());
    }

    // Build TxBlock with a moved deep copy of txpow (TxPoW is non-copyable)
    auto txpow_copy_ptr = txpow.deepCopy();
    auto txblock_ptr = std::make_shared<TxBlock>(tip->getMMR(), *txpow_copy_ptr, chosen_ptrs);
    auto node = std::make_shared<org::minima::database::txpowtree::TxPoWTreeNode>(*txblock_ptr, false);

    // Get the MMR root data
    auto root = node->getMMR().getRoot();

    // We MUST return the TxPoW from the TxBlock, as the TxBlock constructor
    // is responsible for adding the coinbase transaction outputs to it.
    // The original 'txpow' object still has an empty transaction.
    const org::minima::objects::TxPoW& final_txpow_ref = txblock_ptr->getTxPoW();

    // We can't return a reference, so we must make a new deep copy
    // of the TxPoW that is inside the TxBlock.
    std::unique_ptr<org::minima::objects::TxPoW> final_txpow_copy = final_txpow_ref.deepCopy();

    // Now, set the final MMR root and total on THIS correct copy.
    final_txpow_copy->setMMRRoot(root->getData());
    final_txpow_copy->setMMRTotal(root->getValue());

    // Calculate the txpowid / size
    final_txpow_copy->calculateTXPOWID();

    return final_txpow_copy;
}

org::minima::objects::base::MiniData
TxPoWGenerator::getBlockDifficulty(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zParent) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::MinimaLogger;

    // DEBUG: Log entry
    // MinimaLogger::log("=== DEBUG getBlockDifficulty START ===");
    // MinimaLogger::log("Parent Block: " + zParent->getBlockNumber().toString());
    // MinimaLogger::log("Parent TxPoWID: " + zParent->getTxPoW().getTxPoWIDData().to0xString());

    // First 8 blocks check
    if (zParent->getBlockNumber().isLess(MiniNumber::EIGHT())) {
        // MinimaLogger::log("Block < 8, returning MIN_TXPOW_WORK");
        return org::minima::objects::Magic::MIN_TXPOW_WORK;
    }

    // Get start and end blocks
    auto startblock = zParent;
    MiniNumber origstart = startblock->getBlockNumber();
    // MinimaLogger::log("Original start block: " + origstart.toString());

    auto endblock = zParent->getParent(
        org::minima::system::params::GlobalParams::MINIMA_BLOCKS_SPEED_CALC.getAsInt());
    MiniNumber origend = endblock->getBlockNumber();
    // MinimaLogger::log("Original end block: " + origend.toString());
    // MinimaLogger::log("MINIMA_BLOCKS_SPEED_CALC: " + 
    //     std::to_string(org::minima::system::params::GlobalParams::MINIMA_BLOCKS_SPEED_CALC.getAsInt()));

    // Get median time blocks
    startblock = getMedianTimeBlock(startblock);
    endblock = getMedianTimeBlock(endblock);
    
    // MinimaLogger::log("After median - start block: " + startblock->getBlockNumber().toString());
    // MinimaLogger::log("After median - end block: " + endblock->getBlockNumber().toString());
    
    MiniNumber blockdiff = startblock->getBlockNumber().sub(endblock->getBlockNumber());
    // MinimaLogger::log("Block difference: " + blockdiff.toString());

    // Check if same block
    if (startblock->getBlockNumber().isEqual(endblock->getBlockNumber())) {
        // MinimaLogger::log("Start == End, returning parent difficulty");
        return zParent->getTxBlock().getTxPoW().getBlockDifficulty();
    }

    // Get timestamps
    MiniNumber startTime = startblock->getTxPoW().getTimeMilli();
    MiniNumber endTime = endblock->getTxPoW().getTimeMilli();
    
    // MinimaLogger::log("Start block time: " + startTime.toString());
    // MinimaLogger::log("End block time: " + endTime.toString());
    
    MiniNumber timediff = startTime.sub(endTime);
    // MinimaLogger::log("Time difference: " + timediff.toString());

    // Check for time error
    if (timediff.isLessEqual(MiniNumber::ZERO())) {
        MinimaLogger::log("NEGATIVE TIME ERROR - returning parent difficulty"); org::minima::utils::MinimaLogger::log(
            std::string("SERIOUS NEGATIVE TIME ERROR @ ") + zParent->getBlockNumber().toString() +
            " Using latest block diff..");
        return zParent->getTxBlock().getTxPoW().getBlockDifficulty();
    }

    // Calculate speed
    MiniNumber speed = getChainSpeed(startblock, blockdiff);
    // MinimaLogger::log("Chain speed: " + speed.toString());

    // Calculate speed ratio
    MiniNumber targetSpeed = org::minima::system::params::GlobalParams::MINIMA_BLOCK_SPEED;
    // MinimaLogger::log("Target speed (MINIMA_BLOCK_SPEED): " + targetSpeed.toString());
    
    MiniNumber speedratio = targetSpeed.div(speed);
    // MinimaLogger::log("Speed ratio (target/actual): " + speedratio.toString());

    // Apply bounds
    MiniNumber originalSpeedratio = speedratio;
    if (speedratio.isMore(MAX_SPBOUND_DIFFICULTY)) {
        speedratio = MAX_SPBOUND_DIFFICULTY;
        // MinimaLogger::log("Speed ratio clamped to MAX: " + speedratio.toString());
    } else if (speedratio.isLess(MIN_SPBOUND_DIFFICULTY)) {
        speedratio = MIN_SPBOUND_DIFFICULTY;
        // MinimaLogger::log("Speed ratio clamped to MIN: " + speedratio.toString());
    }

    // Get average difficulty (Java BigInteger)
    cpp_int averagedifficulty = getAverageDifficulty_impl(startblock, blockdiff);

    // Calculate new difficulty matching Java exactly:
    // BigDecimal averagedifficultydec.multiply(speedratio.getAsBigDecimal()).toBigInteger()
    // Avoid MiniNumber::mult precision rounding; compute zValue * unscaled / 10^scale directly.
    cpp_int newdiff_int = speedratio.multBigInteger(averagedifficulty);

    MiniData newdiff = cpp_int_to_mini_data(newdiff_int);
    // MinimaLogger::log("New difficulty (MiniData): " + newdiff.to0xString());

    // Check minimum
    if (newdiff.isMore(org::minima::objects::Magic::MIN_TXPOW_WORK)) {
        // MinimaLogger::log("Difficulty > MIN_TXPOW_WORK, clamping to minimum");
        newdiff = org::minima::objects::Magic::MIN_TXPOW_WORK;
    }

    // MinimaLogger::log("Final difficulty: " + newdiff.to0xString());
    // MinimaLogger::log("=== DEBUG getBlockDifficulty END ===");
    
    return newdiff;
}

org::minima::objects::base::MiniNumber
TxPoWGenerator::getChainSpeed(
    const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartBlock,
    const org::minima::objects::base::MiniNumber& zBlocksBack) {

    using org::minima::objects::base::MiniNumber;

    // Past block
    auto pastblock = zStartBlock->getParent(zBlocksBack.getAsInt());

    MiniNumber blockpast = pastblock->getTxPoW().getBlockNumber();
    MiniNumber timepast  = pastblock->getTxPoW().getTimeMilli();

    MiniNumber blocknow  = zStartBlock->getTxPoW().getBlockNumber();
    MiniNumber timenow   = zStartBlock->getTxPoW().getTimeMilli();

    MiniNumber blockdiff = blocknow.sub(blockpast);
    MiniNumber timediff  = timenow.sub(timepast);

    MiniNumber speedmilli = blockdiff.div(timediff);
    MiniNumber speedsecs  = speedmilli.mult(MiniNumber::THOUSAND());

    return speedsecs;
}

std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>
TxPoWGenerator::getMedianTimeBlock(
    const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartBlock) {
    return getMedianTimeBlock(zStartBlock,
                              org::minima::system::params::GlobalParams::MEDIAN_BLOCK_CALC);
}

std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>
TxPoWGenerator::getMedianTimeBlock(
    const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartBlock,
    int zBlocksBack) {

    using org::minima::database::txpowtree::TxPoWTreeNode;
    // using org::minima::utils::MinimaLogger;

    // DEBUG START
    // MinimaLogger::log("  getMedianTimeBlock: input block " + zStartBlock->getBlockNumber().toString() + 
    //                  ", looking back " + std::to_string(zBlocksBack) + " blocks");

    // Start block
    std::shared_ptr<TxPoWTreeNode> current = zStartBlock;

    // Collect blocks
    std::vector<std::shared_ptr<TxPoWTreeNode>> allblocks;
    int counter = 0;
    while (counter < zBlocksBack && current != nullptr) {
        allblocks.push_back(current);
        
        // DEBUG: Log each block collected
        // MinimaLogger::log("    Block " + std::to_string(counter) + ": #" + 
        //                  current->getBlockNumber().toString() + 
        //                  " Time: " + current->getTxPoW().getTimeMilli().toString());
        
        current = current->getParent();
        ++counter;
    }

    // MinimaLogger::log("  Collected " + std::to_string(allblocks.size()) + " blocks");

    // Sort by time milli ascending
    std::sort(allblocks.begin(), allblocks.end(),
        [](const std::shared_ptr<TxPoWTreeNode>& a, const std::shared_ptr<TxPoWTreeNode>& b) {
            return a->getTxPoW().getTimeMilli().compareTo(b->getTxPoW().getTimeMilli()) < 0;
        });

    // DEBUG: Log sorted order
    // MinimaLogger::log("  After sorting by time:");
    // for (size_t i = 0; i < allblocks.size(); i++) {
    //     MinimaLogger::log("    Position " + std::to_string(i) + ": Block #" + 
    //                      allblocks[i]->getBlockNumber().toString() + 
    //                      " Time: " + allblocks[i]->getTxPoW().getTimeMilli().toString());
    // }

    // Middle element
    int middle = static_cast<int>(allblocks.size()) / 2;
    
    // DEBUG: Log result
    // MinimaLogger::log("  Median (position " + std::to_string(middle) + "): Block #" + 
    //                  allblocks[middle]->getBlockNumber().toString() + 
    //                  " Time: " + allblocks[middle]->getTxPoW().getTimeMilli().toString());

    return allblocks[middle];
}

void TxPoWGenerator::precomputeTransactionCoinID(org::minima::objects::Transaction& zTransaction) {
    using org::minima::objects::Coin;
    using org::minima::objects::base::MiniData;

    // Inputs
    auto& inputs = zTransaction.getAllInputs();

    if (inputs.size() == 0) {
        return;
    }

    // First coin
    Coin* firstcoin = inputs[0].get();
    if (!firstcoin) {
        return;
    }

    // ELTOO input?
    bool eltoo = false;
    if (firstcoin->getCoinID().isEqual(Coin::COINID_ELTOO)) {
        eltoo = true;
    }

    // Base modifier
    MiniData basecoinid = firstcoin->getCoinID();

    // Outputs
    auto& outputs = zTransaction.getAllOutputs();
    int num = 0;
    for (auto& outptr : outputs) {
        Coin* output = outptr.get();
        if (!output) {
            ++num;
            continue;
        }

        if (eltoo) {
            // Normal
            output->resetCoinID(Coin::COINID_OUTPUT);
        } else {
            // CoinID = calculate from base and index
            MiniData coinid = zTransaction.calculateCoinID(basecoinid, num);
            output->resetCoinID(coinid);
        }
        ++num;
    }
}

void TxPoWGenerator::main(const std::vector<std::string>& /*zArgs*/) {
    using org::minima::objects::base::MiniNumber;

    std::vector<MiniNumber> nums;
    nums.push_back(MiniNumber::ZERO());
    nums.push_back(MiniNumber::ONE());
    nums.push_back(MiniNumber::TWO());

    std::sort(nums.begin(), nums.end(),
              [](const MiniNumber& a, const MiniNumber& b) {
                  // Java comparator: return o2.compareTo(o1); => descending
                  return b.compareTo(a) < 0;
              });

    // No output in this environment
}

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org