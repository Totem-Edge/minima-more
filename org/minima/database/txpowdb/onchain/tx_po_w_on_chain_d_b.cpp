#include "org/minima/database/txpowdb/onchain/tx_po_w_on_chain_d_b.hpp"

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

#include <sqlite3.h>

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace org {
namespace minima {
namespace database {
namespace txpowdb {
namespace onchain {

using org::minima::utils::MinimaLogger;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

static inline long long nowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

TxPoWOnChainDB::TxPoWOnChainDB()
    : org::minima::utils::SqlDB()
    , mLocalConn(nullptr) {
}

TxPoWOnChainDB::~TxPoWOnChainDB() {
    if (mLocalConn) {
        sqlite3_close(mLocalConn);
        mLocalConn = nullptr;
    }
}

void TxPoWOnChainDB::ensureLocalOpen() {
    if (mLocalConn) return;

    const std::string path = getSQLFile();
    if (path.empty()) {
        throw std::runtime_error("TxPoWOnChainDB: Database file path is empty. Call loadDB() first.");
    }

    int rc = sqlite3_open_v2(path.c_str(), &mLocalConn, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(mLocalConn);
        if (mLocalConn) {
            sqlite3_close(mLocalConn);
            mLocalConn = nullptr;
        }
        throw std::runtime_error("TxPoWOnChainDB: Failed to open SQLite DB: " + err);
    }
    org::minima::utils::SqlDB::applySQLiteTuning(mLocalConn);
}

std::string TxPoWOnChainDB::formatDateString(long long millis) {
    time_t tt = static_cast<time_t>(millis / 1000);
    std::tm tmval{};
#ifdef _WIN32
    localtime_s(&tmval, &tt);
#else
    localtime_r(&tt, &tmval);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmval, "%a %b %d %H:%M:%S %Z %Y");
    return oss.str();
}

void TxPoWOnChainDB::createSQL() {
    std::lock_guard<std::mutex> lock(mMutex);
    ensureLocalOpen();

    const char* create_stmt =
        "CREATE TABLE IF NOT EXISTS `onchaintxpow` ("
        "  `id` INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  `blockid` TEXT NOT NULL,"
        "  `block` INTEGER NOT NULL,"
        "  `txpowid` TEXT NOT NULL,"
        "  `timemilli` INTEGER NOT NULL"
        ");";

    char* errmsg = nullptr;
    int rc = sqlite3_exec(mLocalConn, create_stmt, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown error";
        if (errmsg) sqlite3_free(errmsg);
        throw std::runtime_error("TxPoWOnChainDB::createSQL failed: " + err);
    }
}

void TxPoWOnChainDB::wipeDB() {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        checkOpen();
        ensureLocalOpen();

        const char* drop_stmt = "DROP TABLE IF EXISTS `onchaintxpow`;";
        char* errmsg = nullptr;
        int rc = sqlite3_exec(mLocalConn, drop_stmt, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string err = errmsg ? errmsg : "unknown error";
            if (errmsg) sqlite3_free(errmsg);
            throw std::runtime_error("TxPoWOnChainDB::wipeDB drop failed: " + err);
        }

        // Recreate table
        createSQL();
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
        // Java method throws SQLException; here we swallow after logging.
    }
}

bool TxPoWOnChainDB::addOnChainTxPoW(const std::string& zBlockID,
                                     const MiniNumber& zBlock,
                                     const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        checkOpen();
        ensureLocalOpen();

        const char* sql =
            "INSERT OR IGNORE INTO onchaintxpow "
            "(blockid, block, txpowid, timemilli) VALUES (?, ?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        long long blocknum = zBlock.getAsLong();
        long long tmm = nowMillis();

        sqlite3_bind_text(stmt, 1, zBlockID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(blocknum));
        sqlite3_bind_text(stmt, 3, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(tmm));

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            throw std::runtime_error("execute failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        return true;
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }
    return false;
}

JSONObject TxPoWOnChainDB::getOnChainTxPoW(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(mMutex);

    JSONObject onchain;
    onchain.put("found", false);
    onchain.put("txpowid", zTxPoWID);

    try {
        checkOpen();
        ensureLocalOpen();

        const char* sql =
            "SELECT blockid, block, timemilli FROM onchaintxpow WHERE txpowid=? LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        sqlite3_bind_text(stmt, 1, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const unsigned char* blkid = sqlite3_column_text(stmt, 0);
            long long blocknum = sqlite3_column_int64(stmt, 1);
            long long timemilli = sqlite3_column_int64(stmt, 2);

            onchain.put("found", true);
            onchain.put("blockid", std::string(reinterpret_cast<const char*>(blkid ? blkid : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("block", static_cast<long long>(blocknum));
            onchain.put("timemilli", static_cast<long long>(timemilli));
        }

        sqlite3_finalize(stmt);
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }

    return onchain;
}

JSONArray TxPoWOnChainDB::getInBlockTxPoW(const std::string& zBlockID) {
    std::lock_guard<std::mutex> lock(mMutex);
    JSONArray ret;

    try {
        checkOpen();
        ensureLocalOpen();

        const char* sql =
            "SELECT txpowid, blockid, block, timemilli FROM onchaintxpow WHERE blockid=?;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        sqlite3_bind_text(stmt, 1, zBlockID.c_str(), -1, SQLITE_TRANSIENT);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const unsigned char* txp = sqlite3_column_text(stmt, 0);
            const unsigned char* blkid = sqlite3_column_text(stmt, 1);
            long long blocknum = sqlite3_column_int64(stmt, 2);
            long long timemilli = sqlite3_column_int64(stmt, 3);

            JSONObject onchain;
            onchain.put("txpowid", std::string(reinterpret_cast<const char*>(txp ? txp : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("blockid", std::string(reinterpret_cast<const char*>(blkid ? blkid : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("block", static_cast<long long>(blocknum));
            onchain.put("timemilli", static_cast<long long>(timemilli));

            ret.add(onchain);
        }

        sqlite3_finalize(stmt);
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }

    return ret;
}

JSONArray TxPoWOnChainDB::getInBlockTxPoW(long long zBlock) {
    std::lock_guard<std::mutex> lock(mMutex);
    JSONArray ret;

    try {
        checkOpen();
        ensureLocalOpen();

        const char* sql =
            "SELECT txpowid, blockid, block, timemilli FROM onchaintxpow WHERE block=?;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(zBlock));

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const unsigned char* txp = sqlite3_column_text(stmt, 0);
            const unsigned char* blkid = sqlite3_column_text(stmt, 1);
            long long blocknum = sqlite3_column_int64(stmt, 2);
            long long timemilli = sqlite3_column_int64(stmt, 3);

            JSONObject onchain;
            onchain.put("txpowid", std::string(reinterpret_cast<const char*>(txp ? txp : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("blockid", std::string(reinterpret_cast<const char*>(blkid ? blkid : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("block", static_cast<long long>(blocknum));
            onchain.put("timemilli", static_cast<long long>(timemilli));

            ret.add(onchain);
        }

        sqlite3_finalize(stmt);
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }

    return ret;
}

JSONObject TxPoWOnChainDB::getFirstTxPoW() {
    std::lock_guard<std::mutex> lock(mMutex);
    JSONObject onchain;
    onchain.put("found", false);

    try {
        checkOpen();
        ensureLocalOpen();

        const char* sql =
            "SELECT txpowid, blockid, block, timemilli "
            "FROM onchaintxpow ORDER BY timemilli ASC LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const unsigned char* txp = sqlite3_column_text(stmt, 0);
            const unsigned char* blkid = sqlite3_column_text(stmt, 1);
            long long blocknum     = sqlite3_column_int64(stmt, 2);
            long long timemilli    = sqlite3_column_int64(stmt, 3);

            onchain.put("found", true);
            onchain.put("txpowid", std::string(reinterpret_cast<const char*>(txp ? txp : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("blockid", std::string(reinterpret_cast<const char*>(blkid ? blkid : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("block", static_cast<long long>(blocknum));
            onchain.put("timemilli", static_cast<long long>(timemilli));
            onchain.put("date", formatDateString(timemilli));
        }

        sqlite3_finalize(stmt);
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }

    return onchain;
}

JSONObject TxPoWOnChainDB::getLastTxPoW() {
    std::lock_guard<std::mutex> lock(mMutex);
    JSONObject onchain;
    onchain.put("found", false);

    try {
        checkOpen();
        ensureLocalOpen();

        const char* sql =
            "SELECT txpowid, blockid, block, timemilli "
            "FROM onchaintxpow ORDER BY timemilli DESC LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const unsigned char* txp = sqlite3_column_text(stmt, 0);
            const unsigned char* blkid = sqlite3_column_text(stmt, 1);
            long long blocknum     = sqlite3_column_int64(stmt, 2);
            long long timemilli    = sqlite3_column_int64(stmt, 3);

            onchain.put("found", true);
            onchain.put("txpowid", std::string(reinterpret_cast<const char*>(txp ? txp : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("blockid", std::string(reinterpret_cast<const char*>(blkid ? blkid : reinterpret_cast<const unsigned char*>(""))));
            onchain.put("block", static_cast<long long>(blocknum));
            onchain.put("timemilli", static_cast<long long>(timemilli));
            onchain.put("date", formatDateString(timemilli));
        }

        sqlite3_finalize(stmt);
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }

    return onchain;
}

int TxPoWOnChainDB::getSize() {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        checkOpen();
        ensureLocalOpen();

        const char* sql = "SELECT COUNT(*) AS tot FROM onchaintxpow;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        rc = sqlite3_step(stmt);
        int result = -1;
        if (rc == SQLITE_ROW) {
            result = static_cast<int>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }
    return -1;
}

int TxPoWOnChainDB::cleanDB() {
    return cleanDB(false);
}

int TxPoWOnChainDB::cleanDB(bool zHard) {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        checkOpen();
        ensureLocalOpen();

        long long maxtime = nowMillis() - MAX_ONCHAINSQL_MILLI;
        if (zHard) {
            maxtime = nowMillis() + 100000;
        }

        const char* sql = "DELETE FROM onchaintxpow WHERE timemilli < ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mLocalConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            throw std::runtime_error("prepare failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(maxtime));

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("execute failed: " + std::string(sqlite3_errmsg(mLocalConn)));
        }

        sqlite3_finalize(stmt);
        // Number of rows affected
        return sqlite3_changes(mLocalConn);
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
    }
    return 0;
}

} // namespace onchain
} // namespace txpowdb
} // namespace database
} // namespace minima
} // namespace org

#ifdef MINIMA_BUILD_STANDALONE_TEST
#include <iostream>
int main(int argc, char* argv[]) {
    using namespace org::minima::database::txpowdb::onchain;
    using org::minima::objects::base::MiniNumber;

    try {
        TxPoWOnChainDB db;
        // Provide a valid path before using (example): db.loadDB("testsql.sqlite");
        // db.createSQL();
        // db.addOnChainTxPoW("0xFF", MiniNumber::ONE, "txPowID-01");
        // db.addOnChainTxPoW("0xEE", MiniNumber::TWO, "txPowID-02");
        // db.addOnChainTxPoW("0xDD", MiniNumber::THREE, "txPowID-03");
        // std::cout << db.getOnChainTxPoW("txPowID-01").toString() << std::endl;
        // std::cout << db.getOnChainTxPoW("txPowID-02").toString() << std::endl;
    } catch (const std::exception& ex) {
        org::minima::utils::MinimaLogger::log(ex);
    }
    return 0;
}
#endif