#include "org/minima/utils/sql_d_b.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <iomanip>
#include <iostream>
#include <utility>
#include <cstdlib>
#include <cctype>
#include <string>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace fs = std::filesystem;

namespace org {
namespace minima {
namespace utils {

SqlDB::SqlDB()
    : mSQLConnection(nullptr),
      mEncrypted(false) {
}

SqlDB::~SqlDB() {
    try {
        hardCloseDB();
    } catch (...) {
        // no-throw
    }
}

std::string SqlDB::getSQLFile() const {
    return mSQLFile;
}

void SqlDB::openSQLite(const std::string& path) {
    // Close if currently open
    if (mSQLConnection) {
        sqlite3_close(mSQLConnection);
        mSQLConnection = nullptr;
    }

    int rc = sqlite3_open(path.c_str(), &mSQLConnection);
    if (rc != SQLITE_OK) {
        std::ostringstream oss;
        oss << "Failed to open SQLite DB '" << path << "': " << (mSQLConnection ? sqlite3_errmsg(mSQLConnection) : "no handle");
        if (mSQLConnection) {
            sqlite3_close(mSQLConnection);
            mSQLConnection = nullptr;
        }
        throw std::runtime_error(oss.str());
    }
    // Autocommit is default in SQLite. Apply shared tuning pragmas.
    applySQLiteTuning();

    // Call user create SQL.
    createSQL();
}

void SqlDB::applySQLiteTuning() {
    applySQLiteTuning(mSQLConnection);
}

namespace {
std::string lowerCopy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Path of the "main" database (empty for :memory:).
std::string mainDatabasePath(sqlite3* db) {
    std::string path;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "PRAGMA database_list;", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* name = sqlite3_column_text(stmt, 1);
            const unsigned char* file = sqlite3_column_text(stmt, 2);
            if (name && file && std::string(reinterpret_cast<const char*>(name)) == "main") {
                path = reinterpret_cast<const char*>(file);
            }
        }
        sqlite3_finalize(stmt);
    }
    return path;
}

struct DbTuning {
    int cacheKib;
    int mmapMb;
};

// Role-based cache budget. Six connections at the old 16 MiB each cost ~96 MiB
// of anonymous RSS on a synced mainnet node; small DBs do not need that. Roles
// are identified from the connection's own path, so every caller benefits
// without changing signatures. Override with MINIMA_SQLITE_CACHE_KIB /
// MINIMA_SQLITE_MMAP_MB.
DbTuning tuningForDb(sqlite3* db) {
    DbTuning t{8192, 64};
    const std::string p = lowerCopy(mainDatabasePath(db));
    if (p.find("p2p") != std::string::npos || p.find("userprefs") != std::string::npos) {
        t = {4096, 32};
    } else if (p.find("chaintree") != std::string::npos ||
               p.find("cascade") != std::string::npos ||
               p.find("txpow") != std::string::npos) {
        t = {8192, 64};
    } else if (p.find("archive") != std::string::npos) {
        // The archive DB is loaded even on non-archive nodes, but it is cold
        // there. Give it a large cache only in archive mode.
        t = org::minima::system::params::GeneralParams::ARCHIVE ? DbTuning{16384, 128}
                                                               : DbTuning{4096, 32};
    } else if (p.find("wallet") != std::string::npos) {
        t = {8192, 64};
    }
    if (const char* e = std::getenv("MINIMA_SQLITE_CACHE_KIB")) {
        int v = std::atoi(e);
        if (v > 0) t.cacheKib = v;
    }
    if (const char* e = std::getenv("MINIMA_SQLITE_MMAP_MB")) {
        int v = std::atoi(e);
        if (v >= 0) t.mmapMb = v;
    }
    return t;
}
} // namespace

void SqlDB::applySQLiteTuning(sqlite3* db) {
    if (!db) return;

    const DbTuning t = tuningForDb(db);
    const std::string cacheSql = "PRAGMA cache_size=-" + std::to_string(t.cacheKib) + ";";
    const std::string mmapSql = "PRAGMA mmap_size=" +
        std::to_string(static_cast<long long>(t.mmapMb) * 1024 * 1024) + ";";
    const std::string cacheName = "cache_size=-" + std::to_string(t.cacheKib);
    const std::string mmapName = "mmap_size=" + std::to_string(t.mmapMb) + "MB";

    // One PRAGMA statement per call avoids error-short-circuit and gives clearer logging.
    struct PragmaItem { std::string sql; std::string name; bool required; };
    const PragmaItem pragmas[] = {
        // WAL mode allows readers and writers to coexist; much faster under concurrent load.
        { "PRAGMA journal_mode=WAL;", "journal_mode=WAL", true },
        // NORMAL is safe with WAL and avoids fsync on every transaction commit.
        { "PRAGMA synchronous=NORMAL;", "synchronous=NORMAL", false },
        { cacheSql, cacheName, false },
        // 30 s busy timeout so concurrent writers retry instead of returning SQLITE_BUSY.
        { "PRAGMA busy_timeout=30000;", "busy_timeout=30000", false },
        { mmapSql, mmapName, false },
        // Store temp files in memory where possible.
        { "PRAGMA temp_store=MEMORY;", "temp_store=MEMORY", false },
        // 4 KiB pages match most filesystem block sizes.
        { "PRAGMA page_size=4096;", "page_size=4096", false },
    };

    for (const auto& item : pragmas) {
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, item.sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::string msg = "SQLite tuning pragma " + item.name + " failed: " +
                              (errMsg ? errMsg : sqlite3_errmsg(db));
            if (errMsg) sqlite3_free(errMsg);
            if (item.required) {
                throw std::runtime_error(msg);
            }
            // Non-required pragmas are best-effort; log and continue.
            org::minima::utils::MinimaLogger::log("[SQLDB] " + msg);
        }
    }
}

void SqlDB::loadDB(const std::string& zFile) {
    // Store base file path
    mSQLDBNoMV = zFile;

    // Ensure parent exists
    try {
        fs::path p(zFile);
        fs::path parent = p.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent);
        }
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to create directories for DB path '" << zFile << "': " << e.what();
        throw std::runtime_error(oss.str());
    }

    // Use the exact path as DB file (SQLite)
    mSQLFile = zFile;

    // Open the connection and run createSQL
    openSQLite(mSQLFile);
}

void SqlDB::loadEncryptedSQLDB(const std::string& zFile, const std::string& zPassword) {
    // Note: Stock SQLite3 does not provide encryption. We store these for parity.
    mEncrypted = true;
    mEncryptedPassword = zPassword;

    // Store base file path
    mSQLDBNoMV = zFile;

    // Ensure parent exists
    try {
        fs::path p(zFile);
        fs::path parent = p.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent);
        }
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to create directories for encrypted DB path '" << zFile << "': " << e.what();
        throw std::runtime_error(oss.str());
    }

    // Use the exact path
    mSQLFile = zFile;

    // Open normally (no encryption) and run createSQL
    openSQLite(mSQLFile);
}

bool SqlDB::isOpen() const {
    return mSQLConnection != nullptr;
}

bool SqlDB::checkOpen() {
    return checkOpen(true);
}

bool SqlDB::checkOpen(bool zLogs) {
    bool reopen = false;
    if (mSQLConnection == nullptr) {
        reopen = true;
    }

    if (reopen) {
        if (zLogs) {
            std::cerr << "SqlDB requires re-open: " << fs::path(mSQLDBNoMV).filename().string() << std::endl;
        }
        // Reopen
        openSQLite(mSQLDBNoMV);
    }
    return reopen;
}

void SqlDB::hardCloseDB() {
    // Close the connection if open
    if (mSQLConnection) {
        sqlite3_close(mSQLConnection);
        mSQLConnection = nullptr;
    }
}

void SqlDB::saveDB(bool zCompact) {
    try {
        // std::cerr << "[DEBUG] saveDB called for: " << mSQLFile 
        //           << " compact=" << (zCompact ? "true" : "false") << std::endl;
        
        if (mSQLConnection == nullptr) {
            // std::cerr << "[ERROR] Trying to saveDB on NULL SQLDB: " << mSQLFile << std::endl;
            return;
        }

        // std::cerr << "[DEBUG] Database connection is open: " << mSQLFile << std::endl;

        if (zCompact) {
            // std::cerr << "[DEBUG] Starting compact sequence for: " << mSQLFile << std::endl;
            
            char* errMsg = nullptr;
            int rc;
            
            // Step 1: Optimize database (analyze and update statistics)
            // std::cerr << "[DEBUG] Running PRAGMA optimize..." << std::endl;
            rc = sqlite3_exec(mSQLConnection, "PRAGMA optimize;", nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                // std::cerr << "[ERROR] PRAGMA optimize failed: " << (errMsg ? errMsg : "unknown") << std::endl;
                if (errMsg) sqlite3_free(errMsg);
                errMsg = nullptr;
            }
            
            // Step 2: Checkpoint and truncate WAL
            // std::cerr << "[DEBUG] Checkpointing WAL..." << std::endl;
            rc = sqlite3_exec(mSQLConnection, "PRAGMA wal_checkpoint(TRUNCATE);", 
                              nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                // std::cerr << "[ERROR] WAL checkpoint failed: " << (errMsg ? errMsg : "unknown") << std::endl;
                if (errMsg) sqlite3_free(errMsg);
                errMsg = nullptr;
            }
            
            // Step 3: Try incremental_vacuum instead (doesn't require exclusive lock)
            // std::cerr << "[DEBUG] Attempting incremental_vacuum..." << std::endl;
            rc = sqlite3_exec(mSQLConnection, "PRAGMA incremental_vacuum;", 
                              nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                // std::cerr << "[WARN] incremental_vacuum status: " << (errMsg ? errMsg : "unknown") << std::endl;
                if (errMsg) sqlite3_free(errMsg);
            } else {
                // std::cerr << "[SUCCESS] incremental_vacuum completed" << std::endl;
            }
        }

        // Close connection
        // std::cerr << "[DEBUG] Closing database connection: " << mSQLFile << std::endl;
        int rc = sqlite3_close_v2(mSQLConnection);
        if (rc != SQLITE_OK) {
            // std::cerr << "[ERROR] sqlite3_close_v2 failed with code: " << rc << std::endl;
        } else {
            // std::cerr << "[DEBUG] Database closed successfully: " << mSQLFile << std::endl;
        }
        mSQLConnection = nullptr;
        
    } catch (const std::exception& e) {
        std::cerr << "[EXCEPTION] saveDB error: " << e.what() << std::endl;
    }
}

void SqlDB::backupToFile(const std::string& zBackupFile) {
    backupToFile(zBackupFile, false);
}

void SqlDB::backupToFile(const std::string& zBackupFile, bool /*zGZIP*/) {
    if (mSQLConnection == nullptr) {
        throw std::runtime_error("backupToFile called with no open DB");
    }

    try {
        fs::path bkp(zBackupFile);
        fs::path parent = bkp.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent);
        }
        // If exists, remove
        if (fs::exists(bkp)) {
            fs::remove(bkp);
        }
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to prepare backup file '" << zBackupFile << "': " << e.what();
        throw std::runtime_error(oss.str());
    }

    sqlite3* pFile = nullptr;
    int rc = sqlite3_open(zBackupFile.c_str(), &pFile);
    if (rc != SQLITE_OK) {
        std::ostringstream oss;
        oss << "Failed to open backup DB '" << zBackupFile << "': " << (pFile ? sqlite3_errmsg(pFile) : "no handle");
        if (pFile) sqlite3_close(pFile);
        throw std::runtime_error(oss.str());
    }

    sqlite3_backup* pBackup = sqlite3_backup_init(pFile, "main", mSQLConnection, "main");
    if (!pBackup) {
        std::ostringstream oss;
        oss << "sqlite3_backup_init failed: " << sqlite3_errmsg(pFile);
        sqlite3_close(pFile);
        throw std::runtime_error(oss.str());
    }

    rc = sqlite3_backup_step(pBackup, -1);
    (void)rc; // finalize gives final status.
    rc = sqlite3_backup_finish(pBackup);
    if (rc != SQLITE_OK) {
        std::ostringstream oss;
        oss << "Backup finish failed: " << sqlite3_errmsg(pFile);
        sqlite3_close(pFile);
        throw std::runtime_error(oss.str());
    }

    sqlite3_close(pFile);
}

void SqlDB::restoreFromFile(const std::string& zRestoreFile) {
    restoreFromFile(zRestoreFile, false);
}

void SqlDB::restoreFromFile(const std::string& zRestoreFile, bool /*zGZIP*/) {
    if (mSQLConnection == nullptr) {
        throw std::runtime_error("restoreFromFile called with no open DB");
    }

    // Open source DB
    sqlite3* pSrc = nullptr;
    int rc = sqlite3_open(zRestoreFile.c_str(), &pSrc);
    if (rc != SQLITE_OK) {
        std::ostringstream oss;
        oss << "Failed to open restore DB '" << zRestoreFile << "': " << (pSrc ? sqlite3_errmsg(pSrc) : "no handle");
        if (pSrc) sqlite3_close(pSrc);
        throw std::runtime_error(oss.str());
    }

    // Copy from src (main) to current (main)
    sqlite3_backup* pBackup = sqlite3_backup_init(mSQLConnection, "main", pSrc, "main");
    if (!pBackup) {
        std::ostringstream oss;
        oss << "sqlite3_backup_init (restore) failed: " << sqlite3_errmsg(mSQLConnection);
        sqlite3_close(pSrc);
        throw std::runtime_error(oss.str());
    }

    rc = sqlite3_backup_step(pBackup, -1);
    (void)rc;
    rc = sqlite3_backup_finish(pBackup);
    if (rc != SQLITE_OK) {
        std::ostringstream oss;
        oss << "Restore finish failed: " << sqlite3_errmsg(mSQLConnection);
        sqlite3_close(pSrc);
        throw std::runtime_error(oss.str());
    }

    sqlite3_close(pSrc);
}

static std::string toHex(const unsigned char* data, int len) {
    std::ostringstream oss;
    oss << "0x";
    for (int i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

std::unique_ptr<org::minima::utils::json::JSONObject>
SqlDB::executeGenericSQL(const std::string& zSQL) {
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::json::JSONArray;

    std::lock_guard<std::mutex> guard(mMutex);

    auto results = std::make_unique<JSONObject>();
    results->put("sql", zSQL);

    try {
        // Ensure open
        checkOpen();

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mSQLConnection, zSQL.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::ostringstream oss;
            oss << "ExecuteSQL sql:" << zSQL << " error:prepare: " << sqlite3_errmsg(mSQLConnection);
            std::cerr << oss.str() << std::endl;

            results->put("status", false);
            results->put("count", 0);
            results->put("rows", JSONArray());
            results->put("results", false);
            results->put("error", oss.str());
            if (stmt) sqlite3_finalize(stmt);
            return results;
        }

        int colcount = sqlite3_column_count(stmt);
        bool hasResults = (colcount > 0);

        if (hasResults) {
            JSONArray allrows;
            int counter = 0;

            while (true) {
                rc = sqlite3_step(stmt);
                if (rc == SQLITE_ROW) {
                    ++counter;
                    JSONObject row;

                    for (int i = 0; i < colcount; ++i) {
                        const char* cname = sqlite3_column_name(stmt, i);
                        int ctype = sqlite3_column_type(stmt, i);

                        if (ctype == SQLITE_NULL) {
                            continue; // omit null as in Java
                        }

                        std::string valstr;
                        switch (ctype) {
                            case SQLITE_INTEGER: {
                                long long v = sqlite3_column_int64(stmt, i);
                                valstr = std::to_string(v);
                                break;
                            }
                            case SQLITE_FLOAT: {
                                double v = sqlite3_column_double(stmt, i);
                                std::ostringstream oss;
                                oss << std::setprecision(17) << v;
                                valstr = oss.str();
                                break;
                            }
                            case SQLITE_TEXT: {
                                const unsigned char* txt = sqlite3_column_text(stmt, i);
                                valstr = txt ? reinterpret_cast<const char*>(txt) : "";
                                break;
                            }
                            case SQLITE_BLOB: {
                                const unsigned char* blob = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, i));
                                int blen = sqlite3_column_bytes(stmt, i);
                                valstr = toHex(blob, blen);
                                break;
                            }
                            default: {
                                const unsigned char* txt = sqlite3_column_text(stmt, i);
                                valstr = txt ? reinterpret_cast<const char*>(txt) : "";
                                break;
                            }
                        }

                        row.put(cname ? std::string(cname) : std::string(""), valstr);
                    }

                    allrows.add(row);
                } else if (rc == SQLITE_DONE) {
                    break;
                } else {
                    // Error during stepping
                    std::ostringstream oss;
                    oss << "ExecuteSQL sql:" << zSQL << " error:step: " << sqlite3_errmsg(mSQLConnection);
                    std::cerr << oss.str() << std::endl;

                    results->put("status", false);
                    results->put("count", 0);
                    results->put("rows", JSONArray());
                    results->put("results", false);
                    results->put("error", oss.str());
                    sqlite3_finalize(stmt);
                    return results;
                }
            }

            results->put("status", true);
            results->put("results", true);
            results->put("count", counter);
            results->put("rows", allrows);

        } else {
            // No result set. Execute to completion.
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
                std::ostringstream oss;
                oss << "ExecuteSQL sql:" << zSQL << " error:step-nonselect: " << sqlite3_errmsg(mSQLConnection);
                std::cerr << oss.str() << std::endl;

                results->put("status", false);
                results->put("count", 0);
                results->put("rows", JSONArray());
                results->put("results", false);
                results->put("error", oss.str());
                sqlite3_finalize(stmt);
                return results;
            }

            results->put("status", true);
            results->put("results", false);
        }

        sqlite3_finalize(stmt);

    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "ExecuteSQL sql:" << zSQL << " error:" << e.what();
        std::cerr << oss.str() << std::endl;

        results->put("status", false);
        results->put("count", 0);
        results->put("rows", org::minima::utils::json::JSONArray());
        results->put("results", false);
        results->put("error", oss.str());
    }

    return results;
}

std::unique_ptr<org::minima::utils::json::JSONObject>
SqlDB::convertDataToJSONObject(const org::minima::objects::base::MiniData& zData) {
    using org::minima::objects::base::MiniString;
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::json::parser::JSONParser;
    using org::minima::utils::json::parser::ParseException;

    // Convert bytes -> string
    MiniString str(zData.getBytes());

    // Parse JSON text
    JSONParser parser;
    std::any parsed = parser.parse(str.toString());

    // Try the most common cases without copying non-copyable types
    if (auto pobj = std::any_cast<JSONObject>(&parsed)) {
        return std::make_unique<JSONObject>(*pobj);
    }
    if (auto sp = std::any_cast<std::shared_ptr<JSONObject>>(&parsed)) {
        if (*sp) return std::make_unique<JSONObject>(**sp);
    }
    // If parser stored a unique_ptr, move it out
    if (auto up = std::any_cast<std::unique_ptr<JSONObject>>(&parsed)) {
        return std::move(*up);
    }
    // Raw pointer variant
    if (auto rp = std::any_cast<JSONObject*>(&parsed)) {
        if (*rp) return std::make_unique<JSONObject>(**rp);
    }

    // If not a JSONObject, throw ParseException-like error
    throw ParseException(ParseException::ERROR_UNEXPECTED_TOKEN, parsed);
}

std::unique_ptr<org::minima::objects::base::MiniData>
SqlDB::convertJSONObjectToData(const org::minima::utils::json::JSONObject& zJSON) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniString;

    MiniString str(zJSON.toString());
    auto data = std::make_unique<MiniData>(str.getData());
    return data;
}

} // namespace utils
} // namespace minima
} // namespace org