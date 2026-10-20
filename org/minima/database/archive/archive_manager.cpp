#include "org/minima/database/archive/archive_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <sqlite3.h>
#include <zlib.h>

#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/mini_file.hpp"

namespace org {
namespace minima {
namespace database {
namespace archive {

using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::database::cascade::Cascade;
using org::minima::utils::MinimaLogger;
using org::minima::system::params::GeneralParams;

namespace {
// R1 chunked storage: `blockchunks.data` = "ZCHN" + uint32 rawsize + zlib
// deflate of N concatenated serialized blocks. Per-row (chunkid, offset, len)
// indexes into it. Compressing N blocks together captures cross-block
// redundancy that per-block compression misses (~1.37x vs ~3.4x).
std::vector<std::uint8_t> packChunkBlob(const std::vector<std::uint8_t>& raw) {
    if (raw.empty()) return {};
    const uLongf bound = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> out(8 + static_cast<std::size_t>(bound));
    std::memcpy(out.data(), "ZCHN", 4);
    const std::uint32_t n = static_cast<std::uint32_t>(raw.size());
    std::memcpy(out.data() + 4, &n, sizeof(n));
    uLongf destLen = bound;
    if (compress2(out.data() + 8, &destLen, raw.data(), static_cast<uLong>(raw.size()), 1) != Z_OK) {
        return {};
    }
    out.resize(8 + static_cast<std::size_t>(destLen));
    return out;
}

bool unpackChunkBlob(const void* blob, int blen, std::vector<std::uint8_t>& out) {
    if (!blob || blen < 8) return false;
    const auto* p = static_cast<const std::uint8_t*>(blob);
    if (std::memcmp(p, "ZCHN", 4) != 0) return false;
    std::uint32_t n = 0;
    std::memcpy(&n, p + 4, sizeof(n));
    out.resize(n);
    uLongf destLen = n;
    if (uncompress(out.data(), &destLen, p + 8,
                   static_cast<uLong>(blen - 8)) != Z_OK || destLen != n) {
        out.clear();
        return false;
    }
    return true;
}

bool unpackBlockBlob(const void* blob, int blen, MiniData& out) {
    if (!blob || blen <= 0) return false;
    const auto* p = static_cast<const std::uint8_t*>(blob);
    if (blen >= 8 && std::memcmp(p, "ZBLK", 4) == 0) {
        std::uint32_t n = 0;
        std::memcpy(&n, p + 4, sizeof(n));
        std::vector<std::uint8_t> raw(n);
        uLongf destLen = n;
        if (uncompress(raw.data(), &destLen, p + 8,
                       static_cast<uLong>(blen - 8)) != Z_OK || destLen != n) {
            return false;
        }
        out = MiniData(raw);
        return true;
    }
    out = MiniData(std::vector<std::uint8_t>(p, p + blen));
    return true;
}
} // namespace

ArchiveManager::ArchiveManager()
: SqlDB()
, MAX_KEEP_BLOCKS(2000LL * GeneralParams::NUMBER_DAYS_ARCHIVE)
, mConn(nullptr) {
}

ArchiveManager::~ArchiveManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        if (mConn && !mPending.empty()) {
            flushPendingChunkLocked();
        }
        if (mConn && mInBatch) {
            sqlite3_exec(mConn, "COMMIT;", nullptr, nullptr, nullptr);
            mInBatch = false;
        }
    } catch (...) {
        // best effort on teardown
    }
    if (mStmtInsertRow) { sqlite3_finalize(mStmtInsertRow); mStmtInsertRow = nullptr; }
    if (mStmtInsertChunk) { sqlite3_finalize(mStmtInsertChunk); mStmtInsertChunk = nullptr; }
    if (mConn) {
        sqlite3_close(mConn);
        mConn = nullptr;
    }
}

void ArchiveManager::createSQL() {
    try {
        // Create main table
        const std::string create_syncblock =
            "CREATE TABLE IF NOT EXISTS syncblock ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  txpowid TEXT NOT NULL UNIQUE,"
            "  block INTEGER NOT NULL UNIQUE,"
            "  timemilli INTEGER NOT NULL,"
            "  syncdata BLOB NOT NULL,"
            "  chunkid INTEGER DEFAULT -1,"
            "  offset INTEGER DEFAULT 0,"
            "  len INTEGER DEFAULT 0"
            ");";

        const std::string create_blockchunks =
            "CREATE TABLE IF NOT EXISTS blockchunks ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  data BLOB NOT NULL"
            ");";

        // Create cascade table
        const std::string create_cascade =
            "CREATE TABLE IF NOT EXISTS cascadedata ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  cascadetip INTEGER NOT NULL,"
            "  fulldata BLOB NOT NULL"
            ");";

        // Create index
        const std::string create_index =
            "CREATE INDEX IF NOT EXISTS fastsearch ON syncblock ( txpowid, block );";

        // Execute
        executeGenericSQL(create_syncblock);
        executeGenericSQL(create_blockchunks);
        executeGenericSQL(create_cascade);
        executeGenericSQL(create_index);
    } catch (...) {
        MinimaLogger::log(std::string("ArchiveManager::createSQL - exception executing DDL"));
    }
}

void ArchiveManager::openConnIfNeeded() {
    if (mConn != nullptr) {
        return;
    }
    // Ensure base DB is open (creates file and calls createSQL)
    checkOpen();

    const std::string path = getSQLFile();
    if (path.empty()) {
        throw std::runtime_error("ArchiveManager: SQL file path is empty.");
    }
    int rc = sqlite3_open(path.c_str(), &mConn);
    if (rc != SQLITE_OK) {
        std::string err = "ArchiveManager: sqlite3_open failed: ";
        err += (mConn ? sqlite3_errmsg(mConn) : "unknown");
        if (mConn) {
            sqlite3_close(mConn);
            mConn = nullptr;
        }
        throw std::runtime_error(err);
    }
    org::minima::utils::SqlDB::applySQLiteTuning(mConn);
    ensureSchemaAndStatements();
}

void ArchiveManager::ensureSchemaAndStatements() {
    if (!mConn) return;

    auto tryExec = [this](const char* sql) {
        sqlite3_exec(mConn, sql, nullptr, nullptr, nullptr);
    };
    // R1 migration: existing DBs only have (txpowid, block, timemilli, syncdata).
    tryExec("ALTER TABLE syncblock ADD COLUMN chunkid INTEGER DEFAULT -1;");
    tryExec("ALTER TABLE syncblock ADD COLUMN offset INTEGER DEFAULT 0;");
    tryExec("ALTER TABLE syncblock ADD COLUMN len INTEGER DEFAULT 0;");
    tryExec("CREATE TABLE IF NOT EXISTS blockchunks ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  data BLOB NOT NULL"
            ");");
    tryExec("CREATE INDEX IF NOT EXISTS blockchunk_idx ON syncblock (chunkid);");

    if (!mStmtInsertChunk) {
        const char* sql = "INSERT INTO blockchunks (data) VALUES (?);";
        if (sqlite3_prepare_v2(mConn, sql, -1, &mStmtInsertChunk, nullptr) != SQLITE_OK) {
            MinimaLogger::log(std::string("ArchiveManager: prepare chunk insert failed: ") +
                              (mConn ? sqlite3_errmsg(mConn) : "no db"));
            mStmtInsertChunk = nullptr;
        }
    }
    if (!mStmtInsertRow) {
        const char* sql =
            "INSERT OR IGNORE INTO syncblock (txpowid, block, timemilli, syncdata, chunkid, offset, len) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(mConn, sql, -1, &mStmtInsertRow, nullptr) != SQLITE_OK) {
            MinimaLogger::log(std::string("ArchiveManager: prepare block insert failed: ") +
                              (mConn ? sqlite3_errmsg(mConn) : "no db"));
            mStmtInsertRow = nullptr;
        }
    }
}

std::int64_t ArchiveManager::nowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int ArchiveManager::getSize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT COUNT(*) AS tot FROM syncblock;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("getSize prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return -1;
        }

        int ret = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            ret = sqlite3_column_int(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("getSize step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return ret;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return -1;
    }
}

void ArchiveManager::checkCascadeRequired(const Cascade& zCascade) {
    if (zCascade.getLength() <= 0) {
        return;
    }

    try {
        auto casc = loadCascade();
        if (!casc) {
            // If not present, check correctness before saving
            if (!Cascade::checkCascadeCorrect(zCascade)) {
                MinimaLogger::log("[!] INCONSISTENT Cascade to save in Archive.. not saving..");
                return;
            }

            MinimaLogger::log(std::string("Saving Cascade in ARCHIVEDB.. tip : ")
                              + zCascade.getTip()->getTxPoW().getBlockNumber().toString());
            // Need non-const for serialization
            saveCascade(const_cast<Cascade&>(zCascade));
        } else {
            // Check alignment with archive blocks
            auto last = loadLastBlock();
            if (last) {
                MiniNumber archstart = last->getTxPoW().getBlockNumber();
                auto first = loadFirstBlock();
                if (!first) {
                    MinimaLogger::log(std::string("Cascade already in ARCHIVEDB.. tip : ")
                                      + casc->getTip()->getTxPoW().getBlockNumber().toString());
                    return;
                }
                MiniNumber archend = first->getTxPoW().getBlockNumber();

                MiniNumber cascstart = casc->getTip()->getTxPoW().getBlockNumber();
                // cascstart in [archstart - 1, archend]
                if (cascstart.isMoreEqual(archstart.sub(MiniNumber::ONE())) &&
                    cascstart.isLessEqual(archend)) {
                    // OK
                } else {
                    MinimaLogger::log(std::string("RE-Saving Cascade in ARCHIVEDB.. tip : ")
                                      + zCascade.getTip()->getTxPoW().getBlockNumber().toString());
                    saveCascade(const_cast<Cascade&>(zCascade));
                }
            } else {
                MinimaLogger::log(std::string("Cascade already in ARCHIVEDB.. tip : ")
                                  + casc->getTip()->getTxPoW().getBlockNumber().toString());
            }
        }
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
}

bool ArchiveManager::saveCascade(Cascade& zCascade) {
    // Delete old
    try {
        executeGenericSQL("DELETE FROM cascadedata WHERE id>=0;");
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        // Continue anyway
    }

    try {
        // Serialize to MiniData
        std::unique_ptr<MiniData> cascdata = MiniData::getMiniDataVersion(zCascade);
        if (!cascdata) {
            MinimaLogger::log("saveCascade: MiniData serialization returned null");
            return false;
        }

        checkOpen();
        openConnIfNeeded();

        const char* sql = "INSERT INTO cascadedata (cascadetip, fulldata) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("saveCascade prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return false;
        }

        long long tip = zCascade.getTip()->getTxPoW().getBlockNumber().getAsLong();
        rc = sqlite3_bind_int64(stmt, 1, tip);
        if (rc != SQLITE_OK) {
            MinimaLogger::log("saveCascade bind tip failed");
            sqlite3_finalize(stmt);
            return false;
        }

        const std::vector<std::uint8_t>& bytes = cascdata->getBytes();
        rc = sqlite3_bind_blob(stmt, 2, bytes.data(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            MinimaLogger::log("saveCascade bind blob failed");
            sqlite3_finalize(stmt);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("saveCascade step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return false;
    }
}

std::unique_ptr<Cascade> ArchiveManager::loadCascade() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT fulldata FROM cascadedata ORDER BY cascadetip ASC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadCascade prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        std::unique_ptr<Cascade> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData minisync(v);
                result = Cascade::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadCascade step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

bool ArchiveManager::saveBlock(TxBlock& zBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        return _intSaveBlock(zBlock);
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return false;
    }
}

void ArchiveManager::beginSaveBatch() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();
        if (mConn && !mInBatch) {
            if (sqlite3_exec(mConn, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) == SQLITE_OK) {
                mInBatch = true;
            }
        }
    } catch (...) {
        // best effort
    }
}

void ArchiveManager::endSaveBatch(bool zCommit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!mInBatch) return;
    if (mConn) {
        if (zCommit) {
            flushPendingChunkLocked();
        } else {
            mPending.clear();
        }
        sqlite3_exec(mConn, zCommit ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    mInBatch = false;
}

void ArchiveManager::checkpoint() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();
        if (mConn) {
            if (!mPending.empty()) flushPendingChunkLocked();
            sqlite3_wal_checkpoint_v2(mConn, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr);
        }
    } catch (...) {
        // best effort
    }
}

bool ArchiveManager::_intSaveBlock(TxBlock& zBlock) {
    checkOpen();
    openConnIfNeeded();

    std::unique_ptr<MiniData> syncdata = MiniData::getMiniDataVersion(zBlock);
    if (!syncdata) {
        MinimaLogger::log("saveBlock: MiniData serialization returned null");
        return false;
    }

    PendingBlock pb;
    pb.txpowid = zBlock.getTxPoW().getTxPoWID();
    pb.block = zBlock.getTxPoW().getBlockNumber().getAsLong();
    pb.timemilli = nowMillis();
    pb.data = syncdata->getBytes();
    mPending.push_back(std::move(pb));

    if (mPending.size() >= 256) {
        return flushPendingChunkLocked();
    }
    return true;
}

void ArchiveManager::flushPending() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        if (!mPending.empty()) {
            openConnIfNeeded();
            flushPendingChunkLocked();
        }
        // A partial chunk buffered inside an open batch would otherwise be
        // rolled back when the connection closes. Persist it on shutdown.
        if (mInBatch && mConn) {
            sqlite3_exec(mConn, "COMMIT;", nullptr, nullptr, nullptr);
            mInBatch = false;
        }
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
}

bool ArchiveManager::flushPendingChunkLocked() {
    if (mPending.empty()) return true;
    if (!mConn) openConnIfNeeded();
    if (!mStmtInsertRow || !mStmtInsertChunk) ensureSchemaAndStatements();
    if (!mStmtInsertRow || !mStmtInsertChunk) {
        MinimaLogger::log("ArchiveManager: chunk statements unavailable");
        return false;
    }

    std::vector<std::uint8_t> raw;
    std::vector<std::size_t> offsets;
    std::size_t total = 0;
    for (const auto& pb : mPending) total += pb.data.size();
    raw.reserve(total);
    offsets.reserve(mPending.size());
    for (const auto& pb : mPending) {
        offsets.push_back(raw.size());
        raw.insert(raw.end(), pb.data.begin(), pb.data.end());
    }

    const std::vector<std::uint8_t> packed = packChunkBlob(raw);
    if (packed.empty()) {
        MinimaLogger::log("ArchiveManager: chunk compression failed");
        return false;
    }

    sqlite3_reset(mStmtInsertChunk);
    sqlite3_clear_bindings(mStmtInsertChunk);
    int rc = sqlite3_bind_blob(mStmtInsertChunk, 1, packed.data(),
                               static_cast<int>(packed.size()), SQLITE_TRANSIENT);
    if (rc == SQLITE_OK) rc = sqlite3_step(mStmtInsertChunk);
    if (rc != SQLITE_DONE) {
        MinimaLogger::log(std::string("ArchiveManager: chunk insert failed: ") +
                          (mConn ? sqlite3_errmsg(mConn) : "no db"));
        return false;
    }
    const long long chunkid = sqlite3_last_insert_rowid(mConn);

    static const char kEmpty = 0;
    bool ok = true;
    for (std::size_t i = 0; i < mPending.size(); ++i) {
        const PendingBlock& pb = mPending[i];
        sqlite3_reset(mStmtInsertRow);
        sqlite3_clear_bindings(mStmtInsertRow);
        rc = sqlite3_bind_text(mStmtInsertRow, 1, pb.txpowid.c_str(), -1, SQLITE_TRANSIENT);
        if (rc == SQLITE_OK) rc = sqlite3_bind_int64(mStmtInsertRow, 2, pb.block);
        if (rc == SQLITE_OK) rc = sqlite3_bind_int64(mStmtInsertRow, 3, pb.timemilli);
        if (rc == SQLITE_OK) rc = sqlite3_bind_blob(mStmtInsertRow, 4, &kEmpty, 0, SQLITE_STATIC);
        if (rc == SQLITE_OK) rc = sqlite3_bind_int64(mStmtInsertRow, 5, chunkid);
        if (rc == SQLITE_OK) rc = sqlite3_bind_int64(mStmtInsertRow, 6,
                                                     static_cast<sqlite3_int64>(offsets[i]));
        if (rc == SQLITE_OK) rc = sqlite3_bind_int64(mStmtInsertRow, 7,
                                                     static_cast<sqlite3_int64>(pb.data.size()));
        if (rc != SQLITE_OK) { ok = false; break; }
        rc = sqlite3_step(mStmtInsertRow);
        if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("ArchiveManager: block insert failed: ") +
                              (mConn ? sqlite3_errmsg(mConn) : "no db"));
            ok = false;
            break;
        }
    }
    if (ok) mPending.clear();
    return ok;
}

std::shared_ptr<const std::vector<std::uint8_t>>
ArchiveManager::loadChunkLocked(long long zChunkID) {
    auto it = mChunkCache.find(zChunkID);
    if (it != mChunkCache.end()) return it->second;
    if (!mConn) return nullptr;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(mConn, "SELECT data FROM blockchunks WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
        return nullptr;
    }
    sqlite3_bind_int64(stmt, 1, zChunkID);
    std::vector<std::uint8_t> raw;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        const int blen = sqlite3_column_bytes(stmt, 0);
        if (!unpackChunkBlob(blob, blen, raw)) raw.clear();
    }
    sqlite3_finalize(stmt);
    if (raw.empty()) return nullptr;

    auto chunk = std::make_shared<const std::vector<std::uint8_t>>(std::move(raw));
    if (mChunkCache.size() >= 16) mChunkCache.clear();
    mChunkCache[zChunkID] = chunk;
    return chunk;
}

bool ArchiveManager::extractBlockBytes(const void* blob, int blen, long long zChunkID,
                                       int zOffset, int zLen, MiniData& out) {
    std::vector<std::uint8_t> bytes;
    if (zChunkID >= 0) {
        const auto chunk = loadChunkLocked(zChunkID);
        if (!chunk || zOffset < 0 || zLen < 0 ||
            static_cast<std::size_t>(zOffset) + static_cast<std::size_t>(zLen) > chunk->size()) {
            return false;
        }
        bytes.assign(chunk->begin() + zOffset, chunk->begin() + zOffset + zLen);
    } else {
        // Legacy per-row storage: "ZBLK" + uint32 rawsize + deflate, or raw.
        return unpackBlockBlob(blob, blen, out);
    }
    out = MiniData(bytes);
    return true;
}

std::unique_ptr<TxBlock> ArchiveManager::loadBlock(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT syncdata, chunkid, offset, len FROM syncblock WHERE txpowid=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadBlock prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_text(stmt, 1, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            const int blen = sqlite3_column_bytes(stmt, 0);
            const long long chunkid = sqlite3_column_int64(stmt, 1);
            const int offset = sqlite3_column_int(stmt, 2);
            const int len = sqlite3_column_int(stmt, 3);
            MiniData minisync;
            if (extractBlockBytes(blob, blen, chunkid, offset, len, minisync)) {
                result = TxBlock::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadBlock step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

std::unique_ptr<TxBlock> ArchiveManager::loadFirstBlock() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        // Get lowest block number
        const char* sql1 = "SELECT block FROM syncblock ORDER BY block ASC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql1, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadFirstBlock prepare1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        long long block = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            block = sqlite3_column_int64(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadFirstBlock step1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_finalize(stmt);

        if (block == -1) return nullptr;

        const char* sql2 = "SELECT syncdata, chunkid, offset, len FROM syncblock WHERE block=?;";
        rc = sqlite3_prepare_v2(mConn, sql2, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadFirstBlock prepare2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_int64(stmt, 1, block);

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            const int blen = sqlite3_column_bytes(stmt, 0);
            const long long chunkid = sqlite3_column_int64(stmt, 1);
            const int offset = sqlite3_column_int(stmt, 2);
            const int len = sqlite3_column_int(stmt, 3);
            MiniData minisync;
            if (extractBlockBytes(blob, blen, chunkid, offset, len, minisync)) {
                result = TxBlock::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadFirstBlock step2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

std::unique_ptr<TxBlock> ArchiveManager::loadLastBlock() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        // Get highest block number
        const char* sql1 = "SELECT block FROM syncblock ORDER BY block DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql1, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadLastBlock prepare1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        long long block = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            block = sqlite3_column_int64(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadLastBlock step1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_finalize(stmt);

        if (block == -1) return nullptr;

        const char* sql2 = "SELECT syncdata, chunkid, offset, len FROM syncblock WHERE block=?;";
        rc = sqlite3_prepare_v2(mConn, sql2, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadLastBlock prepare2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_int64(stmt, 1, block);

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            const int blen = sqlite3_column_bytes(stmt, 0);
            const long long chunkid = sqlite3_column_int64(stmt, 1);
            const int offset = sqlite3_column_int(stmt, 2);
            const int len = sqlite3_column_int(stmt, 3);
            MiniData minisync;
            if (extractBlockBytes(blob, blen, chunkid, offset, len, minisync)) {
                result = TxBlock::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadLastBlock step2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

std::vector<std::unique_ptr<TxBlock>>
ArchiveManager::loadSyncBlockRange(const MiniNumber& zStartBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::unique_ptr<TxBlock>> blocks;

    try {
        checkOpen();
        openConnIfNeeded();

        // endblock = start - 256; if <= 1 -> 1
        MiniNumber endblock = zStartBlock.sub(MiniNumber::TWOFIVESIX());
        if (endblock.isLessEqual(MiniNumber::ONE())) {
            endblock = MiniNumber::ONE();
        }

        const char* sql = "SELECT syncdata, chunkid, offset, len FROM syncblock WHERE block<? AND block>=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadSyncBlockRange prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return blocks;
        }

        sqlite3_bind_int64(stmt, 1, zStartBlock.getAsLong());
        sqlite3_bind_int64(stmt, 2, endblock.getAsLong());

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            // Check shutdown
            auto mainptr = org::minima::system::Main::getInstance();
            if (mainptr && mainptr->isShuttingDown()) {
                break;
            }

            const void* blob = sqlite3_column_blob(stmt, 0);
            const int blen = sqlite3_column_bytes(stmt, 0);
            const long long chunkid = sqlite3_column_int64(stmt, 1);
            const int offset = sqlite3_column_int(stmt, 2);
            const int len = sqlite3_column_int(stmt, 3);
            MiniData md;
            if (extractBlockBytes(blob, blen, chunkid, offset, len, md)) {
                auto sb = TxBlock::convertMiniDataVersion(md);
                if (sb) {
                    blocks.push_back(std::move(sb));
                }
            }
        }

        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            MinimaLogger::log(std::string("loadSyncBlockRange step error: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);

        // Sort descending by block number
        std::sort(blocks.begin(), blocks.end(),
                  [](const std::unique_ptr<TxBlock>& a, const std::unique_ptr<TxBlock>& b) {
                      return b->getTxPoW().getBlockNumber().compareTo(a->getTxPoW().getBlockNumber()) < 0;
                  });

        return blocks;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return blocks;
    }
}

MiniNumber ArchiveManager::exists(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT block FROM syncblock WHERE txpowid=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("exists prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return MiniNumber::MINUSONE();
        }

        sqlite3_bind_text(stmt, 1, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);

        MiniNumber result = MiniNumber::MINUSONE();
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            long long block = sqlite3_column_int64(stmt, 0);
            result = MiniNumber(block);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("exists step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return MiniNumber::MINUSONE();
    }
}

std::vector<std::unique_ptr<TxBlock>>
ArchiveManager::loadBlockRange(const MiniNumber& zStartBlock, const MiniNumber& zEndBlock) {
    return loadBlockRange(zStartBlock, zEndBlock, true);
}

std::vector<std::unique_ptr<TxBlock>>
ArchiveManager::loadBlockRange(const MiniNumber& zStartBlock, const MiniNumber& zEndBlock, bool zDescending) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::unique_ptr<TxBlock>> blocks;

    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT syncdata, chunkid, offset, len FROM syncblock WHERE block>? AND block<?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadBlockRange prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return blocks;
        }

        sqlite3_bind_int64(stmt, 1, zStartBlock.getAsLong());
        sqlite3_bind_int64(stmt, 2, zEndBlock.getAsLong());

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            const int blen = sqlite3_column_bytes(stmt, 0);
            const long long chunkid = sqlite3_column_int64(stmt, 1);
            const int offset = sqlite3_column_int(stmt, 2);
            const int len = sqlite3_column_int(stmt, 3);
            MiniData md;
            if (extractBlockBytes(blob, blen, chunkid, offset, len, md)) {
                auto sb = TxBlock::convertMiniDataVersion(md);
                if (sb) {
                    blocks.push_back(std::move(sb));
                }
            }
        }

        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            MinimaLogger::log(std::string("loadBlockRange step error: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);

        if (zDescending) {
            std::sort(blocks.begin(), blocks.end(),
                      [](const std::unique_ptr<TxBlock>& a, const std::unique_ptr<TxBlock>& b) {
                          return b->getTxPoW().getBlockNumber().compareTo(a->getTxPoW().getBlockNumber()) < 0;
                      });
        } else {
            std::sort(blocks.begin(), blocks.end(),
                      [](const std::unique_ptr<TxBlock>& a, const std::unique_ptr<TxBlock>& b) {
                          return a->getTxPoW().getBlockNumber().compareTo(b->getTxPoW().getBlockNumber()) < 0;
                      });
        }

        return blocks;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return blocks;
    }
}

std::unique_ptr<TxBlock>
ArchiveManager::loadBlockFromNumber(const MiniNumber& zBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT syncdata, chunkid, offset, len FROM syncblock WHERE block=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadBlockFromNumber prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_int64(stmt, 1, zBlock.getAsLong());

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            const int blen = sqlite3_column_bytes(stmt, 0);
            const long long chunkid = sqlite3_column_int64(stmt, 1);
            const int offset = sqlite3_column_int(stmt, 2);
            const int len = sqlite3_column_int(stmt, 3);
            MiniData md;
            if (extractBlockBytes(blob, blen, chunkid, offset, len, md)) {
                result = TxBlock::convertMiniDataVersion(md);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadBlockFromNumber step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

int ArchiveManager::checkForCleanDB() {
    if (!GeneralParams::ARCHIVE) {
        return cleanDB();
    }
    return 0;
}

int ArchiveManager::cleanDB() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        // Get highest block number
        const char* sql1 = "SELECT block FROM syncblock ORDER BY block DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql1, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("cleanDB prepare1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return 0;
        }

        long long block = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            block = sqlite3_column_int64(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("cleanDB step1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return 0;
        }
        sqlite3_finalize(stmt);

        if (block == -1) {
            return 0;
        }

        MiniNumber cutoff = MiniNumber(block).sub(MiniNumber(MAX_KEEP_BLOCKS));

        const char* sql2 = "DELETE FROM syncblock WHERE block < ?;";
        rc = sqlite3_prepare_v2(mConn, sql2, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("cleanDB prepare2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return 0;
        }

        sqlite3_bind_int64(stmt, 1, cutoff.getAsLong());

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("cleanDB step2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return 0;
        }

        int changes = sqlite3_changes(mConn);
        sqlite3_finalize(stmt);

        if (changes > 0) {
            sqlite3_exec(mConn,
                "DELETE FROM blockchunks WHERE id NOT IN "
                "(SELECT chunkid FROM syncblock WHERE chunkid >= 0);",
                nullptr, nullptr, nullptr);
            sqlite3_exec(mConn, "PRAGMA incremental_vacuum;", nullptr, nullptr, nullptr);
            mChunkCache.clear();
        }
        return changes;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return 0;
    }
}

void ArchiveManager::hackShut() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Flush buffered blocks, then close our SQLite handle and base DB
    try {
        if (mConn && !mPending.empty()) flushPendingChunkLocked();
        if (mConn && mInBatch) {
            sqlite3_exec(mConn, "COMMIT;", nullptr, nullptr, nullptr);
            mInBatch = false;
        }
    } catch (...) {
        // best effort
    }
    if (mStmtInsertRow) { sqlite3_finalize(mStmtInsertRow); mStmtInsertRow = nullptr; }
    if (mStmtInsertChunk) { sqlite3_finalize(mStmtInsertChunk); mStmtInsertChunk = nullptr; }
    mChunkCache.clear();
    if (mConn) {
        sqlite3_close(mConn);
        mConn = nullptr;
    }
    hardCloseDB();
}

void ArchiveManager::closeAndReopen() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        // Flush buffered blocks before closing/saving
        if (!mPending.empty()) flushPendingChunkLocked();
        if (mInBatch && mConn) {
            sqlite3_exec(mConn, "COMMIT;", nullptr, nullptr, nullptr);
            mInBatch = false;
        }
        if (mStmtInsertRow) { sqlite3_finalize(mStmtInsertRow); mStmtInsertRow = nullptr; }
        if (mStmtInsertChunk) { sqlite3_finalize(mStmtInsertChunk); mStmtInsertChunk = nullptr; }
        mChunkCache.clear();

        // First close the connection and save DB.. no compact
        saveDB(false);

        // Close our own connection if open
        if (mConn) {
            sqlite3_close(mConn);
            mConn = nullptr;
        }

        // Re-open the base DB (no logs)
        checkOpen(false);

        // Our own handle will be reopened lazily on next operation
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
}

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org