#include "org/minima/database/txpowdb/ram/ram_d_b.hpp"
#include "org/minima/database/txpowdb/ram/ram_data.hpp"

#include <chrono>
#include <type_traits>
#include <mutex>

// Full project headers for used types
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/objects/coin.hpp" // For Coin definition
#include "org/minima/objects/witness.hpp" // For Witness definition


static inline std::int64_t now_ms_internal() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

namespace org { namespace minima { namespace database { namespace txpowdb { namespace ram {

//
// FULL implementation of RamData
//
// class RamData {
// public:
//     RamData(std::shared_ptr<org::minima::objects::TxPoW> zTxPoW)
//         : mTxPoW(std::move(zTxPoW)),
//           mLastAccess(now_ms_internal()),
//           mIsOnMainChain(false),
//           mIsInCascade(false) {}

//     std::shared_ptr<org::minima::objects::TxPoW> getTxPoW() const {
//         return mTxPoW;
//     }

//     void updateLastAccess() {
//         mLastAccess = now_ms_internal();
//     }

//     std::int64_t getLastAccess() const {
//         return mLastAccess;
//     }

//     void setOnMainChain(bool zOnChain) {
//         mIsOnMainChain = zOnChain;
//     }

//     bool isOnMainChain() const {
//         return mIsOnMainChain;
//     }

//     void setInCascade(bool zCascader) {
//         mIsInCascade = zCascader;
//     }

//     bool isInCascade() const {
//         return mIsInCascade;
//     }

// private:
//     std::shared_ptr<org::minima::objects::TxPoW> mTxPoW;
//     std::int64_t mLastAccess;
//     bool mIsOnMainChain;
//     bool mIsInCascade;
// };


/*
 * RamDB implementation
 */
std::int64_t RamDB::currentTimeMillis() {
    return now_ms_internal();
}

RamDB::RamDB()
    : MAX_TIME(1000LL * 60 * 60 * org::minima::system::params::GeneralParams::NUMBER_HOURS_RAMTXPOWDB) {
}

bool RamDB::addTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW) {
    if (!zTxPoW) {
        return false;
    }

    const std::string txpid = zTxPoW->getTxPoWID();

    std::unique_lock<std::shared_mutex> lock(mMutex);
    auto it = mTxPoWDB.find(txpid);
    if (it != mTxPoWDB.end()) {
        if (it->second) {
            it->second->updateLastAccess(); 
        }
        return false;
    } else {
        mTxPoWDB.emplace(txpid, std::make_shared<RamData>(zTxPoW)); 
        indexTxPoW(zTxPoW);
    }

    return true;
}

std::unordered_map<std::string, std::shared_ptr<RamData>> RamDB::getCompleteMemPool() const {
    std::shared_lock<std::shared_mutex> lock(mMutex);
    return mTxPoWDB; 
}

bool RamDB::exists(const std::string& zTxPoWID) const {
    std::shared_lock<std::shared_mutex> lock(mMutex);
    return mTxPoWDB.find(zTxPoWID) != mTxPoWDB.end();
}

std::shared_ptr<org::minima::objects::TxPoW> RamDB::getTxPoW(const std::string& zTxPoWID) {
    std::unique_lock<std::shared_mutex> lock(mMutex);
    auto it = mTxPoWDB.find(zTxPoWID);
    if (it != mTxPoWDB.end() && it->second) {
        it->second->updateLastAccess(); 
        return it->second->getTxPoW();  
    }
    return nullptr;
}

void RamDB::remove(const std::string& zTxPoWID) {
    std::unique_lock<std::shared_mutex> lock(mMutex);
    auto it = mTxPoWDB.find(zTxPoWID);
    if (it != mTxPoWDB.end() && it->second) {
        auto txp = it->second->getTxPoW();
        if (txp) {
            unindexTxPoW(txp);
        }
    }
    mTxPoWDB.erase(zTxPoWID);
}

void RamDB::cleanDB() {
    const std::int64_t timecut = currentTimeMillis() - MAX_TIME;

    std::unordered_map<std::string, std::shared_ptr<RamData>> newmap;
    {
        std::unique_lock<std::shared_mutex> lock(mMutex);
        newmap.reserve(mTxPoWDB.size());

        for (auto& kv : mTxPoWDB) {
            const auto& ram = kv.second;
            if (!ram) {
                continue;
            }

            if (ram->getLastAccess() > timecut) { 
                auto txp = ram->getTxPoW(); 
                if (txp) {
                    newmap.emplace(txp->getTxPoWID(), ram);
                }
            }
        }
        mTxPoWDB.swap(newmap);
        rebuildCoinIndex();
    }
}

int RamDB::getSize() const {
    std::shared_lock<std::shared_mutex> lock(mMutex);
    return static_cast<int>(mTxPoWDB.size());
}

void RamDB::wipeRamDB() {
    std::unique_lock<std::shared_mutex> lock(mMutex);
    mTxPoWDB.clear();
    mCoinToTxPoW.clear();
}

void RamDB::clearMainChainTxns() {
    std::unique_lock<std::shared_mutex> lock(mMutex);
    for (auto& kv : mTxPoWDB) {
        if (kv.second) {
            kv.second->setOnMainChain(false); 
        }
    }
}

void RamDB::setOnMainChain(const std::string& zTxPoWID) {
    std::unique_lock<std::shared_mutex> lock(mMutex);
    auto it = mTxPoWDB.find(zTxPoWID);
    if (it != mTxPoWDB.end() && it->second) {
        it->second->setOnMainChain(true); 
    }
}

std::vector<std::shared_ptr<org::minima::objects::TxPoW>> RamDB::getAllUnusedTxns() const {
    std::vector<std::shared_ptr<org::minima::objects::TxPoW>> ret;
    std::shared_lock<std::shared_mutex> lock(mMutex);
    ret.reserve(mTxPoWDB.size());

    for (const auto& kv : mTxPoWDB) {
        const auto& ram = kv.second;
        if (!ram) {
            continue;
        }
        const bool onMain = ram->isOnMainChain(); 
        const bool inCascade = ram->isInCascade(); 
        auto txp = ram->getTxPoW(); 
        if (txp && !onMain && txp->isTransaction() && !inCascade) {
            ret.push_back(txp);
        }
    }

    return ret;
}

void RamDB::setInCascade(const std::string& zTxPoWID) {
    std::unique_lock<std::shared_mutex> lock(mMutex);
    auto it = mTxPoWDB.find(zTxPoWID);
    if (it != mTxPoWDB.end() && it->second) {
        it->second->setInCascade(true); 
        // A cascade TxPoW is excluded from the mempool coin index.
        auto txp = it->second->getTxPoW();
        if (txp) {
            unindexTxPoW(txp);
        }
    }
}

// Index every input CoinID of a transaction TxPoW into mCoinToTxPoW. No-op for
// non-transactions (e.g. blocks).
void RamDB::indexTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW) {
    if (!zTxPoW || !zTxPoW->isTransaction()) {
        return;
    }
    const std::string txpid = zTxPoW->getTxPoWID();
    for (const auto& anycp : zTxPoW->getWitness().getAllCoinProofs()) {
        if (anycp) {
            mCoinToTxPoW[anycp->getCoin().getCoinID().to0xString()] = txpid;
        }
    }
    for (const auto& anycp : zTxPoW->getBurnWitness().getAllCoinProofs()) {
        if (anycp) {
            mCoinToTxPoW[anycp->getCoin().getCoinID().to0xString()] = txpid;
        }
    }
}

// Remove this TxPoW's input CoinIDs from mCoinToTxPoW (only entries that still
// point at this TxPoW, so a conflicting mempool tx's mapping is left intact).
void RamDB::unindexTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW) {
    if (!zTxPoW || !zTxPoW->isTransaction()) {
        return;
    }
    const std::string txpid = zTxPoW->getTxPoWID();
    auto drop = [&](const org::minima::objects::base::MiniData& zCoinID) {
        auto it = mCoinToTxPoW.find(zCoinID.to0xString());
        if (it != mCoinToTxPoW.end() && it->second == txpid) {
            mCoinToTxPoW.erase(it);
        }
    };
    for (const auto& anycp : zTxPoW->getWitness().getAllCoinProofs()) {
        if (anycp) {
            drop(anycp->getCoin().getCoinID());
        }
    }
    for (const auto& anycp : zTxPoW->getBurnWitness().getAllCoinProofs()) {
        if (anycp) {
            drop(anycp->getCoin().getCoinID());
        }
    }
}

// Rebuild the whole index from mTxPoWDB. Caller must hold mMutex.
void RamDB::rebuildCoinIndex() {
    mCoinToTxPoW.clear();
    for (const auto& kv : mTxPoWDB) {
        const auto& ram = kv.second;
        if (!ram || ram->isInCascade()) {
            continue;
        }
        auto txp = ram->getTxPoW();
        if (!txp || !txp->isTransaction()) {
            continue;
        }
        for (const auto& anycp : txp->getWitness().getAllCoinProofs()) {
            if (anycp) {
                mCoinToTxPoW[anycp->getCoin().getCoinID().to0xString()] = kv.first;
            }
        }
        for (const auto& anycp : txp->getBurnWitness().getAllCoinProofs()) {
            if (anycp) {
                mCoinToTxPoW[anycp->getCoin().getCoinID().to0xString()] = kv.first;
            }
        }
    }
}

bool RamDB::checkForCoinID(const org::minima::objects::base::MiniData& zCoinID) const {
    std::shared_lock<std::shared_mutex> lock(mMutex);
    return mCoinToTxPoW.find(zCoinID.to0xString()) != mCoinToTxPoW.end();
}

} } } } } // end namespace org::minima::database::txpowdb::ram