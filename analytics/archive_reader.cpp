#include "archive_reader.hpp"

#include <sqlite3.h>

#include <filesystem>
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
    out.chunks = scalar(db, "SELECT COUNT(*) FROM blockchunks;", ok);
    out.missingChunks = scalar(
        db,
        "SELECT COUNT(*) FROM syncblock s LEFT JOIN blockchunks b "
        "ON s.chunkid = b.id WHERE s.chunkid >= 0 AND b.id IS NULL;",
        ok);

    if (out.count > 0 && out.minBlock >= 0 && out.maxBlock >= out.minBlock) {
        out.complete = (out.count == (out.maxBlock - out.minBlock + 1)) &&
                       (out.distinctIds == out.count) &&
                       (out.missingChunks == 0);
    }

    sqlite3_close(db);
    return out;
}

} // namespace minima::analytics
