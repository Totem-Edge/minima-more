#include "org/minima/database/archive/tx_block_d_b.hpp"

#include <utility>

#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace database {
namespace archive {

TxBlockDB::TxBlockDB() : mTxBlockDB(), mMutex() {}

void TxBlockDB::addTxBlock(const std::shared_ptr<org::minima::objects::TxBlock>& zTxBlock) {
    if (!zTxBlock) {
        // Silently ignore null similar to Java's potential NPE avoidance in calling code,
        // but here we avoid inserting null entries.
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    // Key is TxPoWID of the block
    const org::minima::objects::TxPoW* txpow = &zTxBlock->getTxPoW();
    if (!txpow) {
        return;
    }
    const std::string key = txpow->getTxPoWID();
    mTxBlockDB[key] = zTxBlock;
}

std::shared_ptr<org::minima::objects::TxBlock> TxBlockDB::findTxBlock(const std::string& zTxPowID) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mTxBlockDB.find(zTxPowID);
    if (it == mTxBlockDB.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<std::shared_ptr<org::minima::objects::TxBlock>> TxBlockDB::getChildBlocks(const std::string& zTxPowID) {
    std::lock_guard<std::mutex> lock(mMutex);

    std::vector<std::shared_ptr<org::minima::objects::TxBlock>> ret;
    ret.reserve(mTxBlockDB.size());

    for (const auto& kv : mTxBlockDB) {
        const std::shared_ptr<org::minima::objects::TxBlock>& txblock = kv.second;
        if (!txblock) {
            continue;
        }

        const org::minima::objects::TxPoW* txpow = &txblock->getTxPoW();
        if (!txpow) {
            continue;
        }

        // Is it a child..
        // Compare ParentID 0x string with the given TxPoW ID
        if (txpow->getParentID().to0xString() == zTxPowID) {
            ret.push_back(txblock);
        }
    }

    return ret;
}

void TxBlockDB::clearAll() {
    std::lock_guard<std::mutex> lock(mMutex);
    mTxBlockDB.clear();
}

void TxBlockDB::clearOld(const org::minima::objects::base::MiniNumber& zMinBlock) {
    std::lock_guard<std::mutex> lock(mMutex);

    // int oldsize = mTxBlockDB.size(); // Unused but present in Java for logging

    std::unordered_map<std::string, std::shared_ptr<org::minima::objects::TxBlock>> newDB;
    newDB.reserve(mTxBlockDB.size());

    for (const auto& kv : mTxBlockDB) {
        const auto& txblock = kv.second;
        if (!txblock) {
            continue;
        }
        const org::minima::objects::TxPoW* txpow = &txblock->getTxPoW();
        if (!txpow) {
            continue;
        }
        const org::minima::objects::base::MiniNumber& blocknum = txpow->getBlockNumber();
        if (blocknum.isMoreEqual(zMinBlock)) {
            // Keep this block
            newDB[txpow->getTxPoWID()] = txblock;
        }
    }

    mTxBlockDB = std::move(newDB);
    // MinimaLogger.log("Clear TxBlockDB new size : "+mTxBlockDB.size()+" / "+oldsize);
}

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org