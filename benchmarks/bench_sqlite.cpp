// SQLite tuning benchmark
// Compares bulk INSERT throughput with and without the PureMinima tuning pragmas.
//
// Usage: bench_sqlite <rows> [tuned]
//   rows  : number of rows to insert
//   tuned : 1 = apply tuning pragmas, 0 = leave SQLite defaults

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include <sqlite3.h>

namespace fs = std::filesystem;

static int exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << (err ? err : sqlite3_errmsg(db)) << "\n";
        if (err) sqlite3_free(err);
    }
    return rc;
}

static sqlite3* openDB(const std::string& path, bool tuned) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open DB: " << sqlite3_errstr(rc) << "\n";
        std::exit(1);
    }

    if (tuned) {
        // Mirrors SqlDB::applySQLiteTuning() used by the node
        exec(db, "PRAGMA journal_mode=WAL;");
        exec(db, "PRAGMA synchronous=NORMAL;");
        exec(db, "PRAGMA cache_size=-16384;");
        exec(db, "PRAGMA busy_timeout=30000;");
        exec(db, "PRAGMA mmap_size=268435456;");
        exec(db, "PRAGMA temp_store=MEMORY;");
        exec(db, "PRAGMA page_size=4096;");
    }
    return db;
}

int main(int argc, char** argv) {
    const size_t rows = (argc > 1) ? std::atoll(argv[1]) : 100000;
    const bool tuned = (argc > 2) ? (std::atoi(argv[2]) != 0) : true;

    std::string suffix = tuned ? "tuned" : "default";
    std::string path = "/tmp/bench_sqlite_" + suffix + ".db";

    // Remove any previous DB
    fs::remove_all(path);
    fs::remove_all(path + "-wal");
    fs::remove_all(path + "-shm");

    std::cout << "SQLite benchmark: " << rows << " rows, mode=" << suffix << "\n";

    sqlite3* db = openDB(path, tuned);

    exec(db, "CREATE TABLE IF NOT EXISTS txpow (id TEXT PRIMARY KEY, data BLOB, height INTEGER);");
    exec(db, "CREATE INDEX IF NOT EXISTS idx_height ON txpow(height);");

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    // Prepare deterministic blob data
    std::vector<uint8_t> blob(1024);
    for (size_t i = 0; i < blob.size(); ++i) blob[i] = static_cast<uint8_t>(dist(rng));

    sqlite3_stmt* stmt = nullptr;
    const char* insertSQL = "INSERT INTO txpow (id, data, height) VALUES (?, ?, ?);";
    sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < rows; ++i) {
        std::string id = "txpow_" + std::to_string(i);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 2, blob.data(), static_cast<int>(blob.size()), SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(i));

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Insert failed at row " << i << ": " << sqlite3_errmsg(db) << "\n";
            break;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1e6;
    double rowsPerSec = (rows / (ms / 1000.0));

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total time: " << ms << " ms\n";
    std::cout << "Throughput: " << rowsPerSec << " rows/sec\n";

    // Cleanup
    fs::remove_all(path);
    fs::remove_all(path + "-wal");
    fs::remove_all(path + "-shm");

    return 0;
}
