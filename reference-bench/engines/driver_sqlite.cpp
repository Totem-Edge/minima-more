// RFC-0003 — SQLite driver (baseline, role R1 archive + R2 coin store).
//
// Mirrors the node's existing SQLite usage (embedded, single-writer, batched
// writes) so it is the honest baseline the other engines must beat.
#include <sqlite3.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "interface.hpp"

namespace eng {
namespace {

std::string q(const std::string& p) { return "'" + p + "'"; }

void exec(sqlite3* db, const std::string& sql) {
    char* e = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &e) != SQLITE_OK) {
        std::fprintf(stderr, "sqlite exec failed: %s\n  %s\n", e ? e : "", sql.c_str());
        sqlite3_free(e);
    }
}

void bindBlob(sqlite3_stmt* st, int i, const Bytes& b) {
    sqlite3_bind_blob(st, i, b.data(), static_cast<int>(b.size()), SQLITE_STATIC);
}

Bytes colBlob(sqlite3_stmt* st, int i) {
    const void* p = sqlite3_column_blob(st, i);
    int n = sqlite3_column_bytes(st, i);
    return Bytes(static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n);
}

void configure(sqlite3* db, const OpenOpts& o) {
    exec(db, "PRAGMA journal_mode=" + std::string(o.wal ? "WAL" : "DELETE"));
    exec(db, "PRAGMA synchronous=" + std::string(o.wal ? "NORMAL" : "FULL"));
    exec(db, "PRAGMA temp_store=MEMORY");
    exec(db, "PRAGMA cache_size=-65536");
    exec(db, "PRAGMA mmap_size=268435456");
    exec(db, "PRAGMA busy_timeout=5000");
}

// ---------------------------------------------------------------- archive R1
class SqliteArchive : public ArchiveStore {
public:
    const char* engine() const override { return "sqlite"; }

    void open(const std::string& path, const OpenOpts& o) override {
        path_ = path;
        sqlite3_open(path.c_str(), &db_);
        configure(db_, o);
        exec(db_, "CREATE TABLE IF NOT EXISTS blocks("
                  "num INTEGER PRIMARY KEY, id BLOB, txns INTEGER, payload BLOB)");
        exec(db_, "CREATE INDEX IF NOT EXISTS idx_blocks_id ON blocks(id)");
        exec(db_, "DELETE FROM blocks");
        prep(db_, "INSERT OR REPLACE INTO blocks(num,id,txns,payload) VALUES(?,?,?,?)", &ins_);
        prep(db_, "SELECT payload FROM blocks WHERE num=?", &byNum_);
        prep(db_, "SELECT payload FROM blocks WHERE id=?", &byId_);
        exec(db_, "BEGIN");  // one bulk-load transaction
    }
    void close() override {
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
    }
    void put(const BlockRec& b) override {
        sqlite3_reset(ins_);
        sqlite3_bind_int64(ins_, 1, static_cast<sqlite3_int64>(b.num));
        bindBlob(ins_, 2, b.id);
        sqlite3_bind_int64(ins_, 3, static_cast<sqlite3_int64>(b.txns));
        bindBlob(ins_, 4, b.payload);
        sqlite3_step(ins_);
    }
    void flush() override {
        exec(db_, "COMMIT");   // closes the bulk-load transaction opened in open()
        exec(db_, "ANALYZE");
    }
    bool getByNum(uint64_t num, Bytes& out) override {
        return one(byNum_, num, out, false);
    }
    bool getById(const Bytes& id, Bytes& out) override {
        sqlite3_reset(byId_);
        bindBlob(byId_, 1, id);
        if (sqlite3_step(byId_) == SQLITE_ROW) { out = colBlob(byId_, 0); return true; }
        return false;
    }
    void scanRange(uint64_t a, uint64_t b, bool asc, std::vector<uint64_t>& nums) override {
        std::string sql = "SELECT num FROM blocks WHERE num BETWEEN ? AND ? ORDER BY num " +
                          std::string(asc ? "ASC" : "DESC");
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(a));
        sqlite3_bind_int64(st, 2, static_cast<sqlite3_int64>(b));
        while (sqlite3_step(st) == SQLITE_ROW)
            nums.push_back(static_cast<uint64_t>(sqlite3_column_int64(st, 0)));
        sqlite3_finalize(st);
    }
    bool last(uint64_t& num) override { return agg("SELECT MAX(num) FROM blocks", num); }
    bool first(uint64_t& num) override { return agg("SELECT MIN(num) FROM blocks", num); }
    uint64_t count() override { return cnt("SELECT COUNT(*) FROM blocks"); }
    void analytics(uint64_t& txns, uint64_t& bytes) override {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, "SELECT SUM(txns), SUM(LENGTH(payload)) FROM blocks", -1, &st, nullptr);
        if (sqlite3_step(st) == SQLITE_ROW) {
            txns = static_cast<uint64_t>(sqlite3_column_int64(st, 0));
            bytes = static_cast<uint64_t>(sqlite3_column_int64(st, 1));
        }
        sqlite3_finalize(st);
    }
    void snapshot(const std::string& to) override {
        // VACUUM INTO needs no open statements; the workload is past all reads here.
        if (ins_) { sqlite3_finalize(ins_); ins_ = nullptr; }
        if (byNum_) { sqlite3_finalize(byNum_); byNum_ = nullptr; }
        if (byId_) { sqlite3_finalize(byId_); byId_ = nullptr; }
        exec(db_, "VACUUM INTO " + q(to));
    }

private:
    void prep(sqlite3* db, const char* sql, sqlite3_stmt** st) {
        sqlite3_prepare_v2(db, sql, -1, st, nullptr);
    }
    bool one(sqlite3_stmt* st, uint64_t k, Bytes& out, bool) {
        sqlite3_reset(st);
        sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(k));
        if (sqlite3_step(st) == SQLITE_ROW) { out = colBlob(st, 0); return true; }
        return false;
    }
    bool agg(const char* sql, uint64_t& v) {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, sql, -1, &st, nullptr);
        bool ok = false;
        if (sqlite3_step(st) == SQLITE_ROW) {
            v = static_cast<uint64_t>(sqlite3_column_int64(st, 0));
            ok = true;
        }
        sqlite3_finalize(st);
        return ok;
    }
    uint64_t cnt(const char* sql) { uint64_t v = 0; agg(sql, v); return v; }

    std::string path_;
    sqlite3* db_ = nullptr;
    sqlite3_stmt* ins_ = nullptr, *byNum_ = nullptr, *byId_ = nullptr;
};

// ---------------------------------------------------------------- coin R2
class SqliteCoin : public CoinStore {
public:
    const char* engine() const override { return "sqlite"; }
    void open(const std::string& path, const OpenOpts& o) override {
        path_ = path;
        sqlite3_open(path.c_str(), &db_);
        configure(db_, o);
        exec(db_, "CREATE TABLE IF NOT EXISTS coins("
                  "coinid BLOB PRIMARY KEY, addr BLOB, amount INTEGER, proof BLOB)");
        exec(db_, "CREATE INDEX IF NOT EXISTS idx_coins_addr ON coins(addr)");
        exec(db_, "DELETE FROM coins");
        prep(db_, "INSERT OR REPLACE INTO coins(coinid,addr,amount,proof) VALUES(?,?,?,?)", &ins_);
        prep(db_, "SELECT coinid,amount,proof FROM coins WHERE coinid=?", &get_);
        exec(db_, "BEGIN");
    }
    void close() override { if (db_) { sqlite3_close(db_); db_ = nullptr; } }
    void put(const CoinRec& c) override {
        sqlite3_reset(ins_);
        bindBlob(ins_, 1, c.coinid);
        bindBlob(ins_, 2, c.address);
        sqlite3_bind_int64(ins_, 3, static_cast<sqlite3_int64>(c.amount));
        bindBlob(ins_, 4, c.proof);
        sqlite3_step(ins_);
    }
    void flush() override { exec(db_, "COMMIT"); exec(db_, "ANALYZE"); }
    bool get(const Bytes& coinid, Bytes& out) override {
        sqlite3_reset(get_);
        bindBlob(get_, 1, coinid);
        if (sqlite3_step(get_) == SQLITE_ROW) { out = colBlob(get_, 2); return true; }
        return false;
    }
    void prefixScan(const Bytes& addrPrefix, std::vector<Bytes>& coinids) override {
        Bytes hi = addrPrefix;
        for (size_t i = hi.size(); i-- > 0;) { if (++hi[i] != 0) break; }
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, "SELECT coinid FROM coins WHERE addr>=? AND addr<?", -1, &st, nullptr);
        bindBlob(st, 1, addrPrefix);
        bindBlob(st, 2, hi);
        while (sqlite3_step(st) == SQLITE_ROW) coinids.push_back(colBlob(st, 0));
        sqlite3_finalize(st);
    }
    uint64_t count() override {
        sqlite3_stmt* st = nullptr; sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM coins", -1, &st, nullptr);
        uint64_t v = 0;
        if (sqlite3_step(st) == SQLITE_ROW) v = static_cast<uint64_t>(sqlite3_column_int64(st, 0));
        sqlite3_finalize(st); return v;
    }
    void snapshot(const std::string& to) override {
        if (ins_) { sqlite3_finalize(ins_); ins_ = nullptr; }
        if (get_) { sqlite3_finalize(get_); get_ = nullptr; }
        exec(db_, "VACUUM INTO " + q(to));
    }

private:
    void prep(sqlite3* db, const char* sql, sqlite3_stmt** st) {
        sqlite3_prepare_v2(db, sql, -1, st, nullptr);
    }
    std::string path_;
    sqlite3* db_ = nullptr;
    sqlite3_stmt* ins_ = nullptr, *get_ = nullptr;
};

}  // namespace

std::unique_ptr<ArchiveStore> make_sqlite_archive() { return std::make_unique<SqliteArchive>(); }
std::unique_ptr<CoinStore> make_sqlite_coin() { return std::make_unique<SqliteCoin>(); }

}  // namespace eng
