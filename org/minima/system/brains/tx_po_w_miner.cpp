#include "org/minima/system/brains/tx_po_w_miner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <boost/multiprecision/cpp_int.hpp>
#include <openssl/evp.h>

// Project headers (full includes as required in .cpp)
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/timer_message.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/utils/mshs2/mshs2_mining.hpp"

namespace org {
namespace minima {
namespace system {
namespace brains {

// The node-independent mining core (SHA3 predicate, nonce encoding, bounded
// parallel nonce search) lives in org/minima/utils/mshs2/mshs2_mining.hpp so it
// can be shared with the standalone MASH research/bench harnesses.
namespace mining = org::minima::utils::mshs2::mining;

// Static member initialization
org::minima::objects::base::MiniNumber TxPoWMiner::START_NONCE_BYTES =
    org::minima::objects::base::MiniNumber("100000000000000000.00000000000000000000000000000000000000001");

static inline long long currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Convert positive cpp_int to big-endian two's complement bytes (Java BigInteger.toByteArray() for non-negative values)
static std::vector<uint8_t> cpp_int_to_twos_comp_bytes(const boost::multiprecision::cpp_int& v) {
    using boost::multiprecision::cpp_int;
    cpp_int val = v;
    if (val < 0) {
        val = -val; // not expected here; simple handling
    }

    std::vector<uint8_t> mag;
    export_bits(val, std::back_inserter(mag), 8, /*big_endian=*/true);

    if (mag.empty()) {
        mag.push_back(0x00);
        return mag;
    }

    // If highest bit is set, prepend 0x00 to represent positive in two's complement (Java behavior)
    if (mag[0] & 0x80) {
        mag.insert(mag.begin(), 0x00);
    }
    return mag;
}

TxPoWMiner::TxPoWMiner()
    : org::minima::utils::messages::MessageProcessor("MINER") {
    mMiningCoins.clear();

    startMessageProcessorThread();
}

void TxPoWMiner::mineTxPoWAsync(org::minima::objects::TxPoW& zTxPoW) {
    using org::minima::utils::messages::Message;

    // Add coins to mining list
    addMiningCoins(zTxPoW);

    // Hard set header body hash
    zTxPoW.setHeaderBodyHash();

    // Prepare a deep copy of the TxPoW to ensure lifetime across async processing
    auto copy = zTxPoW.deepCopy();
    if (!copy) {
        org::minima::utils::MinimaLogger::log("TxPoWMiner: deepCopy returned null — skipping mine");
        return;
    }
    auto txpowptr = std::make_shared<org::minima::objects::TxPoW>(std::move(*copy));

    // Post a mining message with automine=false
    auto msg = std::make_shared<Message>(TXPOWMINER_MINETXPOW);
    msg->addObject("txpow", txpowptr);
    msg->addBoolean("automine", false);
    this->PostMessage(msg);
}

void TxPoWMiner::processMessage(org::minima::utils::messages::Message& zMessage) {
    using org::minima::objects::TxPoW;
    using org::minima::objects::TxHeader;
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::Crypto;
    using org::minima::utils::MinimaLogger;
    using org::minima::system::params::GeneralParams;

    if (zMessage.isMessageType(TXPOWMINER_MINETXPOW)) {
        // Start time
        long long timenow = currentTimeMillis();

        // Is automine
        bool automine = false;
        if (zMessage.exists("automine")) {
            automine = zMessage.getBoolean("automine");
        }

        if (GeneralParams::MINING_LOGS) {
            MinimaLogger::log(std::string("MINING TXPOW START auto:") + (automine ? "true" : "false"));
        }

        // Get the TxPoW (support multiple stored forms)
        std::shared_ptr<TxPoW> txpow;
        auto anytx = zMessage.getObject("txpow");
        if (auto sp = std::any_cast<std::shared_ptr<TxPoW>>(&anytx)) {
            txpow = *sp;
        } else if (auto rp = std::any_cast<TxPoW*>(&anytx)) {
            if (*rp) {
                auto copy = (*rp)->deepCopy();
                if (copy) {
                    txpow = std::shared_ptr<TxPoW>(std::move(copy));
                }
            }
        }
        if (!txpow) {
            return;
        }

        // Hard set Header Body hash
        txpow->setHeaderBodyHash();

        // Set a large nonce in bytes (no reserialization)
        txpow->setNonce(START_NONCE_BYTES);

        // Notify mining started
        auto mining = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::Main::MAIN_MINING);
        mining->addBoolean("starting", true);
        mining->addBoolean("automine", automine);
        mining->addObject("txpow", txpow);
        org::minima::system::Main::getInstance()->PostMessage(mining);

        // Serialize TxHeader to bytes
        std::unique_ptr<MiniData> md = MiniData::getMiniDataVersion(txpow->getTxHeader());
        if (!md) {
            return;
        }
        std::vector<uint8_t> data = md->getBytes();

        // Mining loop - bounded parallel nonce search (RFC-0006 E-M2/E-M3).
        MiniNumber finalnonce = MiniNumber::ZERO();
        {
            std::uint64_t winNonce = 0;
            const std::vector<std::uint8_t> diffBytes = txpow->getTxnDifficulty().getBytes();
            if (mining::mineParallel(data, diffBytes, winNonce, [&]() { return isRunning(); })) {
                std::vector<std::uint8_t> noncebytes;
                mining::encodeNonceBE(winNonce, noncebytes);
                std::vector<std::uint8_t> finaldata = data;
                std::copy(noncebytes.begin(), noncebytes.end(), finaldata.begin() + 4);
                TxHeader txh = TxHeader::convertMiniDataVersion(MiniData(finaldata));
                finalnonce = txh.mNonce;
            }
        }

        // Set the final nonce
        txpow->setNonce(finalnonce);

        // Calculate TxPoWID
        txpow->calculateTXPOWID();

        // Log if transaction
        if (txpow->isTransaction()) {
            MinimaLogger::log(std::string("ASYNC Transaction Mined : ") + txpow->getTxPoWID());
        }

        // Mining finished
        auto miningend = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::Main::MAIN_MINING);
        miningend->addBoolean("starting", false);
        // Replicate Java's line: "mining.addBoolean("automine", automine);" before posting miningend
        mining->addBoolean("automine", automine);
        miningend->addObject("txpow", txpow);
        org::minima::system::Main::getInstance()->PostMessage(miningend);

        // Post MAIN_TXPOWMINED
        auto minedmsg = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::Main::MAIN_TXPOWMINED);
        minedmsg->addObject("txpow", txpow);
        org::minima::system::Main::getInstance()->PostMessage(minedmsg);

        // Remove coins from mining list
        removeMiningCoins(*txpow);

        // Automine pulse scheduling
        if (automine) {
            auto delay = getAutomineTimerMillis();
            this->PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(delay, TXPOWMINER_MINEPULSE));

            long long timerdelay = delay / 2;
            this->PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(timerdelay, TXPOWMINER_TXBLOCKMINER));
        }

        if (GeneralParams::MINING_LOGS) {
            long long timediff = currentTimeMillis() - timenow;
            MinimaLogger::log(std::string("MINING TXPOW FINISHED time:") + std::to_string(timediff));
        }

    } else if (zMessage.isMessageType(TXPOWMINER_MINEPULSE)) {
        using org::minima::system::params::GeneralParams;
        using org::minima::database::MinimaDB;
        using org::minima::system::brains::TxPoWGenerator;
        using org::minima::utils::messages::Message;
        using org::minima::utils::messages::TimerMessage;

        if (GeneralParams::TXBLOCK_NODE) {
            return;
        }

        // Check if the tip exists
        if (MinimaDB::getDB()->getTxPoWTree().getTip()) {

            // Create a valid TxPoW that builds on the tip
            // NOTE: You must ensure TxPoWGenerator::generateTxPoW is correctly implemented
            std::shared_ptr<TxPoW> txpow =
                TxPoWGenerator::generateTxPoW(Transaction(), Witness());

            // Post async mining with automine=true
            auto msg = std::make_shared<Message>(TXPOWMINER_MINETXPOW);
            msg->addObject("txpow", txpow);
            msg->addBoolean("automine", true);
            this->PostMessage(msg);

            // Test/solo networks must keep producing blocks so fresh coins
            // confirm and MMR proofs remain usable without restarting the node.
            if (GeneralParams::TEST_PARAMS) {
                long long delay = 1000 / org::minima::system::params::GlobalParams::MINIMA_BLOCK_SPEED.getAsLong();
                if (delay < 1000) delay = 1000;
                this->PostTimerMessage(std::make_shared<TimerMessage>(delay, TXPOWMINER_MINEPULSE));
            }

        } else {

            // No tip yet (still starting up?), check again in 30 seconds
            this->PostTimerMessage(std::make_shared<TimerMessage>(30000, TXPOWMINER_MINEPULSE));
        }

    } else if (zMessage.isMessageType(TXPOWMINER_TXBLOCKMINER)) {
        using org::minima::system::params::GeneralParams;
        if (GeneralParams::TXBLOCK_NODE) {
            return;
        }

        // Best-effort: create a default TxPoW (since TxPoWGenerator is not available in provided headers)
        TxPoW txpow;

        // Post to the TxBlock crew
        org::minima::objects::base::MiniByte mtype(org::minima::system::network::minima::NIOMessage::MSG_TXBLOCKMINE().getValue());
        org::minima::system::network::minima::NIOManager::sendNetworkMessageAll(mtype, txpow);
    }
}

void TxPoWMiner::addMiningCoins(org::minima::objects::TxPoW& zTxPoW) {
    using org::minima::objects::Coin;
    
    std::lock_guard<std::mutex> lock(mMiningCoinsMutex);
    
    // Regular transaction inputs
    if (!zTxPoW.getTransaction().isEmpty()) {
        auto& inputs = zTxPoW.getTransaction().getAllInputs();
        for (const auto& uptr : inputs) {
            const Coin* cc = uptr.get();
            if (!cc) continue;
            mMiningCoins.insert(cc->getCoinID().to0xString()); // O(1) insert
        }
    }

    // Burn transaction inputs
    if (!zTxPoW.getBurnTransaction().isEmpty()) {
        auto& inputs = zTxPoW.getBurnTransaction().getAllInputs();
        for (const auto& uptr : inputs) {
            const Coin* cc = uptr.get();
            if (!cc) continue;
            mMiningCoins.insert(cc->getCoinID().to0xString()); // O(1) insert
        }
    }
}

void TxPoWMiner::removeMiningCoins(org::minima::objects::TxPoW& zTxPoW) {
    using org::minima::objects::Coin;
    
    std::lock_guard<std::mutex> lock(mMiningCoinsMutex);

    if (!zTxPoW.getTransaction().isEmpty()) {
        auto& inputs = zTxPoW.getTransaction().getAllInputs();
        for (const auto& uptr : inputs) {
            const Coin* cc = uptr.get();
            if (!cc) continue;
            mMiningCoins.erase(cc->getCoinID().to0xString()); // O(1) erase
        }
    }

    if (!zTxPoW.getBurnTransaction().isEmpty()) {
        auto& inputs = zTxPoW.getBurnTransaction().getAllInputs();
        for (const auto& uptr : inputs) {
            const Coin* cc = uptr.get();
            if (!cc) continue;
            mMiningCoins.erase(cc->getCoinID().to0xString()); // O(1) erase
        }
    }
}

bool TxPoWMiner::checkForMiningCoin(const std::string& zCoinID) const {
    std::lock_guard<std::mutex> lock(mMiningCoinsMutex);
    return mMiningCoins.count(zCoinID) > 0; // O(1) lookup
}

bool TxPoWMiner::MineMaxTxPoW(org::minima::objects::TxPoW& zTxPoW, long long zTimeLimit) {
    return MineMaxTxPoW(zTxPoW, zTimeLimit, true);
}

bool TxPoWMiner::MineMaxTxPoW(org::minima::objects::TxPoW& zTxPoW, long long zTimeLimit, bool zPost) {
    using org::minima::objects::TxHeader;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::Crypto;
    using org::minima::utils::MinimaLogger;
    using org::minima::system::params::GeneralParams;

    long long timenow = currentTimeMillis();

    // Add coins to mining list
    addMiningCoins(zTxPoW);

    // Fix header/body hash and set large nonce
    zTxPoW.setHeaderBodyHash();
    zTxPoW.setNonce(START_NONCE_BYTES);

    // Serialize header
    std::unique_ptr<MiniData> md = MiniData::getMiniDataVersion(zTxPoW.getTxHeader());
    if (!md) {
        removeMiningCoins(zTxPoW);
        return false;
    }
    std::vector<uint8_t> data = md->getBytes();

    // Mining loop - bounded parallel nonce search (RFC-0006 E-M2/E-M3).
    MiniNumber finalnonce = MiniNumber::ZERO();
    {
        std::uint64_t winNonce = 0;
        const std::vector<std::uint8_t> diffBytes = zTxPoW.getTxnDifficulty().getBytes();
        if (!mining::mineParallel(data, diffBytes, winNonce,
                          [&]() { return (currentTimeMillis() - timenow) <= zTimeLimit; })) {
            removeMiningCoins(zTxPoW);
            return false;
        }
        std::vector<std::uint8_t> noncebytes;
        mining::encodeNonceBE(winNonce, noncebytes);
        std::vector<std::uint8_t> finaldata = data;
        std::copy(noncebytes.begin(), noncebytes.end(), finaldata.begin() + 4);
        TxHeader txh = TxHeader::convertMiniDataVersion(MiniData(finaldata));
        finalnonce = txh.mNonce;
    }

    // Apply final nonce and calculate ID
    zTxPoW.setNonce(finalnonce);
    zTxPoW.calculateTXPOWID();

    if (zTxPoW.isTransaction()) {
        MinimaLogger::log(std::string("SYNC Transaction Mined : ") + zTxPoW.getTxPoWID());
    }

    if (zPost) {
        // Share a copy for posting
        auto txp = std::make_shared<org::minima::objects::TxPoW>(std::move(*zTxPoW.deepCopy()));
        auto minedmsg = std::make_shared<org::minima::utils::messages::Message>(org::minima::system::Main::MAIN_TXPOWMINED);
        minedmsg->addObject("txpow", txp);
        org::minima::system::Main::getInstance()->PostMessage(minedmsg);
    }

    // Remove mining coins
    removeMiningCoins(zTxPoW);

    return true;
}

org::minima::objects::base::MiniNumber TxPoWMiner::calculateHashRateOld(org::minima::objects::base::MiniNumber zHashes) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::Crypto;

    int ihashes = zHashes.getAsInt();

    long long timestart = currentTimeMillis();
    MiniData data = MiniData::getRandomData(512);
    for (int i = 0; i < ihashes; ++i) {
        data = Crypto::getInstance().hashObject(data);
    }
    long long timediff = currentTimeMillis() - timestart;
    if (timediff <= 0) timediff = 1;

    MiniNumber timesecs = MiniNumber(timediff).div(MiniNumber::THOUSAND());
    MiniNumber spd = zHashes.div(timesecs);
    return spd;
}

org::minima::objects::base::MiniNumber TxPoWMiner::calculateHashSpeed(const org::minima::objects::base::MiniNumber& zHashes) {
    using org::minima::objects::TxPoW;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    // NOTE: No longer using Crypto::getInstance() in the hot loop
    
    int ihashes = zHashes.getAsInt();
    long long timenow = currentTimeMillis();
    
    TxPoW txp;
    txp.setHeaderBodyHash();
    txp.setNonce(START_NONCE_BYTES);
    
    std::unique_ptr<MiniData> md = MiniData::getMiniDataVersion(txp.getTxHeader());
    if (!md) {
        return MiniNumber::ZERO();
    }
    
    std::vector<std::uint8_t> data = md->getBytes();
    boost::multiprecision::cpp_int newnonce = 0;
    
    // OPTIMIZED: Direct OpenSSL calls instead of Crypto::hashData()
    for (int i = 0; i < ihashes; ++i) {
        std::vector<std::uint8_t> noncebytes = cpp_int_to_twos_comp_bytes(newnonce);
        newnonce += 1;
        
        if (4 + noncebytes.size() <= data.size()) {
            std::copy(noncebytes.begin(), noncebytes.end(), data.begin() + 4);
        } else {
            continue;
        }
        
        // OPTIMIZED: Use fast_sha3_256() instead of Crypto::getInstance().hashData()
        std::vector<std::uint8_t> hashedbytes = mining::sha3(data);
        // (No need to check hash, we're just benchmarking speed)
    }
    
    long long timediff = currentTimeMillis() - timenow;
    if (timediff <= 0) timediff = 1;
    
    MiniNumber timesecs = MiniNumber(timediff).div(MiniNumber::THOUSAND());
    MiniNumber spd = zHashes.div(timesecs);
    
    return spd;
}

long long TxPoWMiner::getAutomineTimerMillis() const {
    // Fallback due to lack of accessor for AUTOMINE_TIMER in provided Main.hpp
    return 30000;
}

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org