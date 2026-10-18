#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"

#include <algorithm>
#include <chrono>
#include <regex>
#include <vector>

#include "sqlite3.h"

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp" // Needed for static member definition
#include "org/minima/objects/base/mini_byte.hpp"

//
// ### FIX 2 ###
// Added include for the full Transaction definition
//
#include "org/minima/objects/transaction.hpp"

namespace org {
namespace minima {
namespace database {
namespace txpowdb {
namespace sql {

// Static members
std::int64_t TxPoWSqlDB::MAX_SQL_MILLI =
    1000LL * 60LL * 60LL * 24LL * org::minima::system::params::GeneralParams::NUMBER_DAYS_SQLTXPOWDB;

// This definition will now work
org::minima::objects::base::MiniNumber TxPoWSqlDB::MAX_RELEVANT_TXPOW =
    org::minima::objects::base::MiniNumber::HUNDRED();

bool TxPoWSqlDB::LOG_ADD_BLOCKS = false;
bool TxPoWSqlDB::LOG_ADD_TRANSACTION = false;
bool TxPoWSqlDB::DISABLE_BATCH = false;

TxPoWSqlDB::TxPoWSqlDB()
    : mConn(nullptr)
    , mStmtInsert(nullptr)
    , mStmtSelectByID(nullptr)
    , mStmtSelectChildren(nullptr)
    , mStmtTotalTxpow(nullptr)
    , mStmtDeleteOldNotRelevant(nullptr)
    , mStmtExists(nullptr)
    , mStmtSelectRelevant(nullptr)
    , mStmtSelectRelevantSize(nullptr)
    , mStmtSelectTopTxpow(nullptr)
    , mStmtSelectTopTxpowSize(nullptr) {
}

TxPoWSqlDB::~TxPoWSqlDB() {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    finalizeStatements();
    closeConnection();
}

void TxPoWSqlDB::openConnectionIfNeeded() {
    if (mConn) return;
    const std::string file = getSQLFile();
    int rc = sqlite3_open(file.c_str(), &mConn);
    if (rc != SQLITE_OK) {
        org::minima::utils::MinimaLogger::log(std::string("SQLite open failed: ") + sqlite3_errmsg(mConn));
        if (mConn) {
            sqlite3_close(mConn);
            mConn = nullptr;
        }
        throw std::runtime_error("Failed to open SQLite DB");
    }
    org::minima::utils::SqlDB::applySQLiteTuning(mConn);
}

void TxPoWSqlDB::execSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(mConn, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "unknown";
        if (errMsg) sqlite3_free(errMsg);
        org::minima::utils::MinimaLogger::log(std::string("SQLite exec error: ") + err + " for SQL: " + sql);
        throw std::runtime_error("SQLite exec failed");
    }
}

void TxPoWSqlDB::beginBatchLocked() {
    if (DISABLE_BATCH || mBatchOpen || !mConn) return;
    // Deferred transaction; the first INSERT upgrades it to a write transaction.
    int rc = sqlite3_exec(mConn, "BEGIN", nullptr, nullptr, nullptr);
    if (rc == SQLITE_OK) {
        mBatchOpen = true;
        mBatchCount = 0;
        mBatchStartMs = currentTimeMillis();
    }
    // If BEGIN fails (already in a transaction) just proceed unbatched.
}

void TxPoWSqlDB::commitBatchLocked() {
    if (!mBatchOpen) return;
    if (mConn) {
        sqlite3_exec(mConn, "COMMIT", nullptr, nullptr, nullptr);
    }
    mBatchOpen = false;
    mBatchCount = 0;
}

void TxPoWSqlDB::flushBatchLocked() {
    if (!mBatchOpen) return;
    if (mBatchCount >= BATCH_MAX_ROWS ||
        (currentTimeMillis() - mBatchStartMs) >= BATCH_MAX_MS) {
        commitBatchLocked();
    }
}

void TxPoWSqlDB::flushBatch() {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    commitBatchLocked();
}

void TxPoWSqlDB::saveDB(bool zCompact) {
    // Commit pending inserts first, then defer to the base (checkpoint/close).
    std::lock_guard<std::mutex> lock(mLocalMutex);
    commitBatchLocked();
    org::minima::utils::SqlDB::saveDB(zCompact);
}

void TxPoWSqlDB::finalizeStatements() {
    auto fin = [](sqlite3_stmt*& st) {
        if (st) { sqlite3_finalize(st); st = nullptr; }
    };
    fin(mStmtInsert);
    fin(mStmtSelectByID);
    fin(mStmtSelectChildren);
    fin(mStmtTotalTxpow);
    fin(mStmtDeleteOldNotRelevant);
    fin(mStmtExists);
    fin(mStmtSelectRelevant);
    fin(mStmtSelectRelevantSize);
    fin(mStmtSelectTopTxpow);
    fin(mStmtSelectTopTxpowSize);
}

void TxPoWSqlDB::closeConnection() {
    // Never close with an open write transaction - commit pending inserts first.
    commitBatchLocked();
    if (mConn) {
        sqlite3_close(mConn);
        mConn = nullptr;
    }
}

void TxPoWSqlDB::prepareStatements() {
    // Prepare all SQL statements
    const char* sqlInsert =
        "INSERT OR IGNORE INTO txpow "
        "( txpowid, isblock, istransaction, parentid, timemilli, txpowdata, isrelevant ) "
        "VALUES ( ?, ?, ?, ?, ?, ?, ? )";
    const char* sqlSelectByID = "SELECT txpowdata FROM txpow WHERE txpowid=?";
    const char* sqlSelectChildren = "SELECT txpowid FROM txpow WHERE isblock=1 AND parentid=?";
    const char* sqlTotal = "SELECT COUNT(*) AS tot FROM txpow";
    const char* sqlDeleteOld = "DELETE FROM txpow WHERE timemilli < ? AND isrelevant=0";
    const char* sqlExists = "SELECT txpowid FROM txpow WHERE txpowid=?";
    const char* sqlSelectRelevant = "SELECT txpowdata FROM txpow WHERE isrelevant=1 ORDER BY timemilli DESC LIMIT ? OFFSET ?";
    const char* sqlSelectRelevantSize = "SELECT COUNT(*) AS tot FROM txpow WHERE isrelevant=1";
    const char* sqlSelectTopTxpow = "SELECT txpowdata FROM txpow ORDER BY timemilli DESC LIMIT ? OFFSET ?";
    const char* sqlSelectTopTxpowSize = "SELECT COUNT(*) AS tot FROM txpow WHERE timemilli > ?";

    auto prep = [&](const char* zsql, sqlite3_stmt** st) {
        int rc = sqlite3_prepare_v2(mConn, zsql, -1, st, nullptr);
        if (rc != SQLITE_OK) {
            org::minima::utils::MinimaLogger::log(std::string("Prepare failed: ") + sqlite3_errmsg(mConn) + " SQL: " + zsql);
            throw std::runtime_error("SQLite prepare failed");
        }
    };

    prep(sqlInsert, &mStmtInsert);
    prep(sqlSelectByID, &mStmtSelectByID);
    prep(sqlSelectChildren, &mStmtSelectChildren);
    prep(sqlTotal, &mStmtTotalTxpow);
    prep(sqlDeleteOld, &mStmtDeleteOldNotRelevant);
    prep(sqlExists, &mStmtExists);
    prep(sqlSelectRelevant, &mStmtSelectRelevant);
    prep(sqlSelectRelevantSize, &mStmtSelectRelevantSize);
    prep(sqlSelectTopTxpow, &mStmtSelectTopTxpow);
    prep(sqlSelectTopTxpowSize, &mStmtSelectTopTxpowSize);
}

void TxPoWSqlDB::createSQL() {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        openConnectionIfNeeded();
        const std::string create =
            "CREATE TABLE IF NOT EXISTS txpow ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  txpowid TEXT NOT NULL UNIQUE,"
            "  isblock INTEGER NOT NULL,"
            "  istransaction INTEGER NOT NULL,"
            "  parentid TEXT NOT NULL,"
            "  timemilli INTEGER NOT NULL,"
            "  txpowdata BLOB NOT NULL,"
            "  isrelevant INTEGER NOT NULL"
            ");";
        execSQL(create);
        const std::string index = "CREATE INDEX IF NOT EXISTS fastsearch ON txpow ( txpowid, parentid );";
        execSQL(index);
        finalizeStatements();
        prepareStatements();
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
    }
}

void TxPoWSqlDB::wipeDB() {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        // Commit any pending inserts before DDL (SQLite forbids DDL in a txn is
        // not the rule, but dropping mid-batch would be surprising here).
        commitBatchLocked();
        finalizeStatements();
        execSQL("DROP TABLE IF EXISTS txpow;");
        const std::string create =
            "CREATE TABLE IF NOT EXISTS txpow ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  txpowid TEXT NOT NULL UNIQUE,"
            "  isblock INTEGER NOT NULL,"
            "  istransaction INTEGER NOT NULL,"
            "  parentid TEXT NOT NULL,"
            "  timemilli INTEGER NOT NULL,"
            "  txpowdata BLOB NOT NULL,"
            "  isrelevant INTEGER NOT NULL"
            ");";
        execSQL(create);
        prepareStatements();
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        throw;
    }
}

std::string TxPoWSqlDB::sanitizeWhere(const std::string& where) {
    std::string lower = where;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    lower = std::regex_replace(lower, std::regex(";"), "");
    lower = std::regex_replace(lower, std::regex("\\bupdate\\b"), "");
    lower = std::regex_replace(lower, std::regex("\\bdelete\\b"), "");
    lower = std::regex_replace(lower, std::regex("\\binsert\\b"), "");
    return lower;
}

int TxPoWSqlDB::customSizeQuery(const std::string& zWhereCondition) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        std::string where = sanitizeWhere(zWhereCondition);
        std::string sql = "SELECT Count(*) AS tot FROM txpow WHERE " + where;
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            org::minima::utils::MinimaLogger::log(std::string("Prepare custom query failed: ") + sqlite3_errmsg(mConn));
            if (stmt) sqlite3_finalize(stmt);
            return -1;
        }
        int res = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            res = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return res;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return -1;
    }
}

std::int64_t TxPoWSqlDB::currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

bool TxPoWSqlDB::addTxPoW(const org::minima::objects::TxPoW& zTxPoW, bool zIsRelevant) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        if (LOG_ADD_BLOCKS && zTxPoW.isBlock()) {
            org::minima::utils::MinimaLogger::log(std::string("[+] BLOCK ADDED TO TXPOWDB : ") + zTxPoW.getBlockNumber().toString());
        }

        if (LOG_ADD_TRANSACTION && zTxPoW.isTransaction()) {
            // This line will now work
            org::minima::utils::MinimaLogger::log(std::string("[+] TRANSACTION ADDED TO TXPOWDB : ") +
                zTxPoW.getBlockNumber().toString() + " - " + zTxPoW.getTransaction().toJSON().toString());
        }

        checkOpen();
        openConnectionIfNeeded();

        // Start (or continue) a batched write transaction for bursty inserts.
        beginBatchLocked();

        auto mdptr = org::minima::objects::base::MiniData::getMiniDataVersion(const_cast<org::minima::objects::TxPoW&>(zTxPoW));
        if (!mdptr) {
            org::minima::utils::MinimaLogger::log("Failed to convert TxPoW to MiniData");
            return false;
        }
        const auto& bytes = mdptr->getBytes();

        sqlite3_reset(mStmtInsert);
        sqlite3_clear_bindings(mStmtInsert);

        std::string txpowid = zTxPoW.getTxPoWID();
        sqlite3_bind_text(mStmtInsert, 1, txpowid.c_str(), -1, SQLITE_TRANSIENT);

        int isblock = org::minima::objects::base::MiniByte(zTxPoW.isBlock()).getValue();
        sqlite3_bind_int(mStmtInsert, 2, isblock);

        int istran = org::minima::objects::base::MiniByte(zTxPoW.isTransaction()).getValue();
        sqlite3_bind_int(mStmtInsert, 3, istran);

        std::string parent = zTxPoW.getParentID().to0xString();
        sqlite3_bind_text(mStmtInsert, 4, parent.c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_bind_int64(mStmtInsert, 5, static_cast<sqlite3_int64>(currentTimeMillis()));

        if (!bytes.empty()) {
            sqlite3_bind_blob(mStmtInsert, 6, bytes.data(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_blob(mStmtInsert, 6, nullptr, 0, SQLITE_TRANSIENT);
        }

        sqlite3_bind_int(mStmtInsert, 7, zIsRelevant ? 1 : 0);

        int rc = sqlite3_step(mStmtInsert);
        if (rc != SQLITE_DONE) {
            org::minima::utils::MinimaLogger::log(std::string("Insert failed: ") + sqlite3_errmsg(mConn));
            return false;
        }

        // Count this write and commit the batch if it is full / old enough.
        ++mBatchCount;
        flushBatchLocked();

        return true;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return false;
    }
}

std::unique_ptr<org::minima::objects::TxPoW> TxPoWSqlDB::getTxPoW(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtSelectByID);
        sqlite3_clear_bindings(mStmtSelectByID);
        sqlite3_bind_text(mStmtSelectByID, 1, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(mStmtSelectByID);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(mStmtSelectByID, 0);
            int len = sqlite3_column_bytes(mStmtSelectByID, 0);
            std::vector<std::uint8_t> data;
            if (blob && len > 0) {
                const std::uint8_t* b = static_cast<const std::uint8_t*>(blob);
                data.assign(b, b + len);
            }
            org::minima::objects::base::MiniData minitxp(data);
            auto txp = org::minima::objects::TxPoW::convertMiniDataVersion(minitxp);
            return txp;
        }
        return nullptr;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return nullptr;
    }
}

std::vector<std::string> TxPoWSqlDB::getChildBlocks(const std::string& zParentTxPoWID) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    std::vector<std::string> txpows;
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtSelectChildren);
        sqlite3_clear_bindings(mStmtSelectChildren);
        sqlite3_bind_text(mStmtSelectChildren, 1, zParentTxPoWID.c_str(), -1, SQLITE_TRANSIENT);

        int rc;
        while ((rc = sqlite3_step(mStmtSelectChildren)) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(mStmtSelectChildren, 0);
            if (txt) {
                txpows.emplace_back(reinterpret_cast<const char*>(txt));
            }
        }
        return txpows;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return txpows;
    }
}

int TxPoWSqlDB::getRelevantSize() {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtSelectRelevantSize);
        sqlite3_clear_bindings(mStmtSelectRelevantSize);
        int rc = sqlite3_step(mStmtSelectRelevantSize);
        if (rc == SQLITE_ROW) {
            return sqlite3_column_int(mStmtSelectRelevantSize, 0);
        }
        return -1;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return -1;
    }
}

std::vector<std::unique_ptr<org::minima::objects::TxPoW>> TxPoWSqlDB::getAllRelevant(int zLimit) {
    return getAllRelevant(zLimit, 0);
}

std::vector<std::unique_ptr<org::minima::objects::TxPoW>> TxPoWSqlDB::getAllRelevant(int zLimit, int zOffset) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    std::vector<std::unique_ptr<org::minima::objects::TxPoW>> txpows;
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtSelectRelevant);
        sqlite3_clear_bindings(mStmtSelectRelevant);
        sqlite3_bind_int(mStmtSelectRelevant, 1, zLimit);
        sqlite3_bind_int(mStmtSelectRelevant, 2, zOffset);
        int rc;
        while ((rc = sqlite3_step(mStmtSelectRelevant)) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(mStmtSelectRelevant, 0);
            int len = sqlite3_column_bytes(mStmtSelectRelevant, 0);
            std::vector<std::uint8_t> data;
            if (blob && len > 0) {
                const std::uint8_t* b = static_cast<const std::uint8_t*>(blob);
                data.assign(b, b + len);
            }
            org::minima::objects::base::MiniData minitxp(data);
            auto txp = org::minima::objects::TxPoW::convertMiniDataVersion(minitxp);
            if (txp) {
                txpows.emplace_back(std::move(txp));
            }
        }
        return txpows;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return txpows;
    }
}

std::vector<std::unique_ptr<org::minima::objects::TxPoW>> TxPoWSqlDB::getLatestTxPoW(int zLimit, int zOffset) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    std::vector<std::unique_ptr<org::minima::objects::TxPoW>> txpows;
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtSelectTopTxpow);
        sqlite3_clear_bindings(mStmtSelectTopTxpow);
        sqlite3_bind_int(mStmtSelectTopTxpow, 1, zLimit);
        sqlite3_bind_int(mStmtSelectTopTxpow, 2, zOffset);
        int rc;
        while ((rc = sqlite3_step(mStmtSelectTopTxpow)) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(mStmtSelectTopTxpow, 0);
            int len = sqlite3_column_bytes(mStmtSelectTopTxpow, 0);
            std::vector<std::uint8_t> data;
            if (blob && len > 0) {
                const std::uint8_t* b = static_cast<const std::uint8_t*>(blob);
                data.assign(b, b + len);
            }
            org::minima::objects::base::MiniData minitxp(data);
            auto txp = org::minima::objects::TxPoW::convertMiniDataVersion(minitxp);
            if (txp) {
                txpows.emplace_back(std::move(txp));
            }
        }
        return txpows;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return txpows;
    }
}

int TxPoWSqlDB::getLatestTxPoWSize(std::int64_t zMinMilliTime) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtSelectTopTxpowSize);
        sqlite3_clear_bindings(mStmtSelectTopTxpowSize);
        sqlite3_bind_int64(mStmtSelectTopTxpowSize, 1, static_cast<sqlite3_int64>(zMinMilliTime));
        int rc = sqlite3_step(mStmtSelectTopTxpowSize);
        if (rc == SQLITE_ROW) {
            return sqlite3_column_int(mStmtSelectTopTxpowSize, 0);
        }
        return -1;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return -1;
    }
}

int TxPoWSqlDB::getSize() {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtTotalTxpow);
        sqlite3_clear_bindings(mStmtTotalTxpow);
        int rc = sqlite3_step(mStmtTotalTxpow);
        if (rc == SQLITE_ROW) {
            return sqlite3_column_int(mStmtTotalTxpow, 0);
        }
        return -1;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return -1;
    }
}

bool TxPoWSqlDB::exists(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        sqlite3_reset(mStmtExists);
        sqlite3_clear_bindings(mStmtExists);
        sqlite3_bind_text(mStmtExists, 1, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(mStmtExists);
        return (rc == SQLITE_ROW);
    } catch (const std::exception&) {
        return false;
    }
}

int TxPoWSqlDB::cleanDB() {
    return cleanDB(false);
}

int TxPoWSqlDB::cleanDB(bool zHard) {
    std::lock_guard<std::mutex> lock(mLocalMutex);
    try {
        checkOpen();
        openConnectionIfNeeded();
        std::int64_t maxtime = currentTimeMillis() - MAX_SQL_MILLI;
        if (zHard) {
            if (LOG_ADD_BLOCKS) {
                org::minima::utils::MinimaLogger::log("[+] TxPoWDB wipe clean..");
            }
            maxtime = currentTimeMillis() + 100000;
        }
        sqlite3_reset(mStmtDeleteOldNotRelevant);
        sqlite3_clear_bindings(mStmtDeleteOldNotRelevant);
        sqlite3_bind_int64(mStmtDeleteOldNotRelevant, 1, static_cast<sqlite3_int64>(maxtime));
        int rc = sqlite3_step(mStmtDeleteOldNotRelevant);
        if (rc != SQLITE_DONE) {
            org::minima::utils::MinimaLogger::log(std::string("Delete failed: ") + sqlite3_errmsg(mConn));
            return 0;
        }
        int num = sqlite3_changes(mConn);
        if (LOG_ADD_BLOCKS) {
            org::minima::utils::MinimaLogger::log(std::string("[+] TxPoWDB clean up deleted : ") + std::to_string(num));
        }
        return num;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
        return 0;
    }
}

void TxPoWSqlDB::closeAndReopen() {
    // Phase 1: commit pending inserts and close (under the lock).
    {
        std::lock_guard<std::mutex> lock(mLocalMutex);
        try {
            commitBatchLocked();
            org::minima::utils::SqlDB::saveDB(false); // close base connection
            finalizeStatements();
            closeConnection();
        } catch (const std::exception& e) {
            org::minima::utils::MinimaLogger::log(e.what());
            return;
        }
    }
    // Phase 2: reopen. checkOpen() -> createSQL() takes mLocalMutex itself, so
    // it must NOT be called while holding the lock (that self-deadlocks).
    try {
        checkOpen(false); // reopen base connection + createSQL() re-prepares
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e.what());
    }
}

} // namespace sql
} // namespace txpowdb
} // namespace database
} // namespace minima
} // namespace org