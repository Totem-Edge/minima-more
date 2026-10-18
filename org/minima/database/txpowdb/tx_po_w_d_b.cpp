#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"

#include <utility>
#include <exception>
#include <memory>

#include "org/minima/database/txpowdb/ram/ram_d_b.hpp"
#include "org/minima/database/txpowdb/ram/ram_data.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/database/txpowdb/onchain/tx_po_w_on_chain_d_b.hpp"

#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"

#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace database {
namespace txpowdb {

// Destructor and move definitions (PIMPL fix)
TxPoWDB::~TxPoWDB() = default;
TxPoWDB::TxPoWDB(TxPoWDB&&) noexcept = default;
TxPoWDB& TxPoWDB::operator=(TxPoWDB&&) noexcept = default;

TxPoWDB::TxPoWDB()
    : mRamDB(std::make_unique<org::minima::database::txpowdb::ram::RamDB>())
    , mSqlDB(std::make_unique<org::minima::database::txpowdb::sql::TxPoWSqlDB>())
    , mOnChainDB(std::make_unique<org::minima::database::txpowdb::onchain::TxPoWOnChainDB>()) {
}

void TxPoWDB::loadSQLDB(const std::filesystem::path& zFile) {
    // Set the SQL DB base file
    mSqlDB->loadDB(zFile.string());

    // Create a subfolder for the onchain data
    std::filesystem::path onchainfile = zFile.parent_path() / "onchain";
    mOnChainDB->loadDB(onchainfile.string());
}

void TxPoWDB::hardCloseSQLDB() {
    mSqlDB->flushBatch(); // commit any pending batched inserts before closing
    mSqlDB->hardCloseDB();
    mOnChainDB->hardCloseDB();
}

void TxPoWDB::saveDB(bool zCompact) {
    mSqlDB->saveDB(zCompact);
    mOnChainDB->saveDB(zCompact);
}

bool TxPoWDB::addTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW) {
    if (!zTxPoW) {
        return false;
    }

    // Get the ID
    std::string txpid = zTxPoW->getTxPoWID();

    // Post a message to MAIN to store in MySQL..
    try {
        auto* main = org::minima::system::Main::getInstance();
        if (main) {
            auto msg = std::make_shared<org::minima::utils::messages::Message>(
                org::minima::system::Main::MAIN_AUTOBACKUP_TXPOW);
            msg->addObject("txpow", zTxPoW);
            main->PostMessage(msg);
        }
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(std::string("STORESQl TxPoW : ") + exc.what());
    } catch (...) {
        org::minima::utils::MinimaLogger::log("STORESQl TxPoW : unknown exception");
    }

    // Do we have it already in RAM
    if (!mRamDB->exists(txpid)) {
        // Add it to the RAM
        mRamDB->addTxPoW(zTxPoW);
    }

    // Is it in SQL
    if (!mSqlDB->exists(txpid)) {
        // Is this TxPoW relevant
        //
        // FIX: Removed dereference '*' from getWallet()
        //
        bool relevant = org::minima::system::brains::TxPoWSearcher::checkTxPoWRelevant(
            *zTxPoW, org::minima::database::MinimaDB::getDB()->getWallet());

        // Add it to the SQL..
        mSqlDB->addTxPoW(*zTxPoW, relevant);

        return relevant;
    }

    // Check if relevant
    //
    // FIX: Removed dereference '*' from getWallet()
    //
    return org::minima::system::brains::TxPoWSearcher::checkTxPoWRelevant(
        *zTxPoW, org::minima::database::MinimaDB::getDB()->getWallet());
}

void TxPoWDB::addSQLTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW) {
    if (!zTxPoW) {
        return;
    }

    // Is it in SQL
    if (!mSqlDB->exists(zTxPoW->getTxPoWID())) {
        // Is this TxPoW relevant
        //
        // FIX: Removed dereference '*' from getWallet()
        //
        bool relevant = org::minima::system::brains::TxPoWSearcher::checkTxPoWRelevant(
            *zTxPoW, org::minima::database::MinimaDB::getDB()->getWallet());

        // Add it to SQL
        mSqlDB->addTxPoW(*zTxPoW, relevant);
    }
}

std::shared_ptr<org::minima::objects::TxPoW>
TxPoWDB::getTxPoW(const std::string& zTxPoWID) {
    // First check the fast RAM DB
    auto txp = mRamDB->getTxPoW(zTxPoWID);

    // Old TxPoW?
    if (!txp) {
        // Check SQL
        std::unique_ptr<org::minima::objects::TxPoW> up = mSqlDB->getTxPoW(zTxPoWID);
        if (up) {
            // Convert unique_ptr -> shared_ptr for outward API
            txp = std::shared_ptr<org::minima::objects::TxPoW>(std::move(up));
        }
    }

    return txp;
}

std::vector<std::shared_ptr<org::minima::objects::TxPoW>>
TxPoWDB::getAllTxPoW(const std::vector<std::string>& zTxPoWID) {
    std::vector<std::shared_ptr<org::minima::objects::TxPoW>> ret;
    ret.reserve(zTxPoWID.size());

    // Cycle through the list
    for (const auto& child : zTxPoWID) {
        auto txp = getTxPoW(child);
        if (txp) {
            ret.push_back(std::move(txp));
        }
    }

    return ret;
}

bool TxPoWDB::exists(const std::string& zTxPoWID) {
    // Is it in RAM
    bool ex = mRamDB->exists(zTxPoWID);

    // If not, check SQL
    if (!ex) {
        ex = mSqlDB->exists(zTxPoWID);
    }
    return ex;
}

std::vector<std::shared_ptr<org::minima::objects::TxPoW>>
TxPoWDB::getChildBlocks(const std::string& zParentTxPoWID) {
    // Ask the SQL for all children first
    std::vector<std::string> children = mSqlDB->getChildBlocks(zParentTxPoWID);

    // Now get them all
    return getAllTxPoW(children);
}

int TxPoWDB::getRamSize() {
    return mRamDB->getSize();
}

int TxPoWDB::getSqlSize() {
    return mSqlDB->getSize();
}

std::filesystem::path TxPoWDB::getSqlFile() {
    return mSqlDB->getSQLFile();
}

org::minima::database::txpowdb::sql::TxPoWSqlDB* TxPoWDB::getSQLDB() {
    return mSqlDB.get();
}

org::minima::database::txpowdb::onchain::TxPoWOnChainDB* TxPoWDB::getOnChainDB() {
    return mOnChainDB.get();
}

void TxPoWDB::cleanDBRAM() {
    mRamDB->cleanDB();
}

void TxPoWDB::wipeDBRAM() {
    mRamDB->wipeRamDB();
}

void TxPoWDB::cleanDBSQL() {
    mSqlDB->cleanDB();
    mOnChainDB->cleanDB();
}

void TxPoWDB::clearMainChainTxns() {
    mRamDB->clearMainChainTxns();
}

void TxPoWDB::setOnMainChain(const std::string& zTxPoWID) {
    mRamDB->setOnMainChain(zTxPoWID);
}

void TxPoWDB::setInCascade(const std::string& zTxPoWID) {
    mRamDB->setInCascade(zTxPoWID);
}

std::vector<std::shared_ptr<org::minima::objects::TxPoW>> TxPoWDB::getAllUnusedTxns() {
    return mRamDB->getAllUnusedTxns();
}

void TxPoWDB::removeMemPoolTxPoW(const std::string& zTxPoWID) {
    mRamDB->remove(zTxPoWID);
}

bool TxPoWDB::checkMempoolCoins(const org::minima::objects::base::MiniData& zCoinID) {
    return mRamDB->checkForCoinID(zCoinID);
}

std::unordered_map<std::string, std::shared_ptr<org::minima::database::txpowdb::ram::RamData>>
TxPoWDB::getCompleteMemPool() {
    return mRamDB->getCompleteMemPool();
}

} // namespace txpowdb
} // namespace database
} // namespace minima
} // namespace org