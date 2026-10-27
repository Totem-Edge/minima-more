#include "archive_reader.hpp"

#include <sqlite3.h>

#include <cstring>
#include <filesystem>

#include <zlib.h>
#include <iostream>

namespace minima::analytics {

namespace {

std::int64_t scalar(sqlite3* db, const char* sql, bool& ok) {
    sqlite3_stmt* stmt = nullptr;
    std::int64_t value = -1;
    ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int64(stmt, 0);
        ok = true;
    }
    if (stmt) sqlite3_finalize(stmt);
    return value;
}

bool tableExists(sqlite3* db, const char* name) {
    sqlite3_stmt* stmt = nullptr;
    bool found = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;",
                           -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        found = sqlite3_step(stmt) == SQLITE_ROW;
    }
    if (stmt) sqlite3_finalize(stmt);
    return found;
}

// `table`/`column` are internal literals, so the PRAGMA is built directly
// (PRAGMA does not accept bound parameters).
bool columnExists(sqlite3* db, const char* table, const char* column) {
    const std::string sql = std::string("PRAGMA table_info(") + table + ");";
    sqlite3_stmt* stmt = nullptr;
    bool found = false;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* name = sqlite3_column_text(stmt, 1);
            if (name && std::strcmp(reinterpret_cast<const char*>(name), column) == 0) {
                found = true;
                break;
            }
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    return found;
}

} // namespace

ArchiveStats readArchiveStats(const std::string& path) {
    ArchiveStats out;
    out.path = path;

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        out.sizeBytes = static_cast<std::int64_t>(std::filesystem::file_size(path, ec));
    }

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        out.error = db ? sqlite3_errmsg(db) : "open failed";
        std::cerr << "archive open failed: " << out.error << "\n";
        if (db) sqlite3_close(db);
        return out;
    }
    sqlite3_busy_timeout(db, 2000);

    bool ok = false;
    out.count = scalar(db, "SELECT COUNT(*) FROM syncblock;", ok);
    if (!ok) {
        out.error = sqlite3_errmsg(db);
        std::cerr << "archive query failed: " << out.error << "\n";
        sqlite3_close(db);
        return out;
    }
    out.exists = true;
    out.minBlock = scalar(db, "SELECT MIN(block) FROM syncblock;", ok);
    out.maxBlock = scalar(db, "SELECT MAX(block) FROM syncblock;", ok);
    out.distinctIds = scalar(db, "SELECT COUNT(DISTINCT txpowid) FROM syncblock;", ok);

    // A pre-v0.5 archive stores each block in its own `syncdata` blob and has
    // neither the chunkid column nor the blockchunks table. Both forms are
    // valid; only the completeness checks differ.
    const bool hasChunkId = columnExists(db, "syncblock", "chunkid");
    const bool hasChunks = tableExists(db, "blockchunks");
    out.legacyStorage = !(hasChunkId && hasChunks);

    if (hasChunks) {
        out.chunks = scalar(db, "SELECT COUNT(*) FROM blockchunks;", ok);
    }
    if (hasChunkId && hasChunks) {
        out.missingChunks = scalar(
            db,
            "SELECT COUNT(*) FROM syncblock s LEFT JOIN blockchunks b "
            "ON s.chunkid = b.id WHERE s.chunkid >= 0 AND b.id IS NULL;",
            ok);
    }

    if (out.count > 0 && out.minBlock >= 0 && out.maxBlock >= out.minBlock) {
        out.complete = (out.count == (out.maxBlock - out.minBlock + 1)) &&
                       (out.distinctIds == out.count) &&
                       (!hasChunkId || (hasChunks && out.missingChunks == 0));
    }

    sqlite3_close(db);
    return out;
}

} // namespace minima::analytics

namespace minima::analytics {

namespace {

bool unpackChunk(const void* blob, int len, std::vector<std::uint8_t>& out) {
    const auto* p = static_cast<const std::uint8_t*>(blob);
    if (!p || len < 8 || std::memcmp(p, "ZCHN", 4) != 0) return false;
    std::uint32_t rawSize = 0;
    std::memcpy(&rawSize, p + 4, sizeof(rawSize));
    out.resize(rawSize);
    uLongf destLen = rawSize;
    if (uncompress(out.data(), &destLen, p + 8, static_cast<uLong>(len - 8)) != Z_OK ||
        destLen != rawSize) {
        out.clear();
        return false;
    }
    return true;
}

bool unpackLegacy(const void* blob, int len, std::vector<std::uint8_t>& out) {
    const auto* p = static_cast<const std::uint8_t*>(blob);
    if (!p || len <= 0) return false;
    if (len >= 8 && std::memcmp(p, "ZBLK", 4) == 0) {
        std::uint32_t rawSize = 0;
        std::memcpy(&rawSize, p + 4, sizeof(rawSize));
        out.resize(rawSize);
        uLongf destLen = rawSize;
        if (uncompress(out.data(), &destLen, p + 8, static_cast<uLong>(len - 8)) != Z_OK ||
            destLen != rawSize) {
            out.clear();
            return false;
        }
        return true;
    }
    out.assign(p, p + len);
    return true;
}

} // namespace

bool forEachArchiveBlock(const std::string& path, std::int64_t fromHeight,
                         const BlockSink& sink, std::string& error) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        error = db ? sqlite3_errmsg(db) : "open failed";
        if (db) sqlite3_close(db);
        return false;
    }
    sqlite3_busy_timeout(db, 2000);

    sqlite3_stmt* stmt = nullptr;
    // Legacy archives have no chunkid/offset/len; the constant columns keep the
    // row shape identical so the -1 chunkid path reads `syncdata` directly.
    const bool chunked = columnExists(db, "syncblock", "chunkid") &&
                         tableExists(db, "blockchunks");
    const char* sql = chunked
                          ? "SELECT block, syncdata, chunkid, offset, len FROM syncblock "
                            "WHERE block > ? ORDER BY block;"
                          : "SELECT block, syncdata, -1, 0, 0 FROM syncblock "
                            "WHERE block > ? ORDER BY block;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_int64(stmt, 1, fromHeight);

    std::int64_t cachedChunkId = -1;
    std::vector<std::uint8_t> cachedChunk;
    std::vector<std::uint8_t> bytes;
    bool ok = true;
    std::int64_t lastHeight = fromHeight;

    while (true) {
        const int rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            // Any mid-scan failure (SQLITE_BUSY while the node writes a chunk,
            // SQLITE_IOERR, corruption) used to fall out of the ROW loop and be
            // reported as a clean end-of-scan, silently truncating the index at
            // an arbitrary height.
            error = "archive read failed after block " + std::to_string(lastHeight) +
                    " (sqlite rc=" + std::to_string(rc) + "): " + sqlite3_errmsg(db);
            ok = false;
            break;
        }

        const std::int64_t height = sqlite3_column_int64(stmt, 0);
        const std::int64_t chunkId = sqlite3_column_int64(stmt, 2);

        if (chunkId >= 0) {
            if (chunkId != cachedChunkId) {
                sqlite3_stmt* cstmt = nullptr;
                if (sqlite3_prepare_v2(db, "SELECT data FROM blockchunks WHERE id = ?;",
                                       -1, &cstmt, nullptr) != SQLITE_OK) {
                    error = sqlite3_errmsg(db);
                    ok = false;
                    break;
                }
                sqlite3_bind_int64(cstmt, 1, chunkId);
                if (sqlite3_step(cstmt) != SQLITE_ROW ||
                    !unpackChunk(sqlite3_column_blob(cstmt, 0),
                                 sqlite3_column_bytes(cstmt, 0), cachedChunk)) {
                    error = "missing or invalid archive chunk " + std::to_string(chunkId) +
                            " at block " + std::to_string(height);
                    sqlite3_finalize(cstmt);
                    ok = false;
                    break;
                }
                sqlite3_finalize(cstmt);
                cachedChunkId = chunkId;
            }
            const int offset = sqlite3_column_int(stmt, 3);
            const int len = sqlite3_column_int(stmt, 4);
            if (offset < 0 || len < 0 ||
                static_cast<std::size_t>(offset + len) > cachedChunk.size()) {
                error = "invalid archive chunk offset at block " + std::to_string(height) +
                        " (chunk " + std::to_string(chunkId) + ", offset " +
                        std::to_string(offset) + ", len " + std::to_string(len) + ")";
                ok = false;
                break;
            }
            bytes.assign(cachedChunk.begin() + offset, cachedChunk.begin() + offset + len);
        } else {
            if (!unpackLegacy(sqlite3_column_blob(stmt, 1),
                              sqlite3_column_bytes(stmt, 1), bytes)) {
                error = "invalid legacy archive row";
                ok = false;
                break;
            }
        }

        lastHeight = height;
        if (!sink(height, bytes)) break;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

} // namespace minima::analytics
