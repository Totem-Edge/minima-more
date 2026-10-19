// RFC-0003 — DuckDB driver (role R1 archive analytics + R2 `duckdbcoins`).
//
// Columnar OLAP: bulk load via the Appender (the point of the engine), point gets
// via SQL (expected to lose the point-lookup race: per-statement overhead),
// analytics via native aggregates. BLOBs are passed as hex-escaped literals so
// arbitrary bytes survive (DuckDB validates UTF-8 for plain strings). Guarded by
// HAVE_DUCKDB.
#include "interface.hpp"

#ifdef HAVE_DUCKDB
#include "duckdb.hpp"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace eng {
namespace {

std::string S(const Bytes& b) { return std::string(reinterpret_cast<const char*>(b.data()), b.size()); }
Bytes B(const std::string& s) { return Bytes(s.begin(), s.end()); }
std::string blob(const Bytes& b) {  // SQL BLOB literal with hex escapes
    std::string s = "'";
    char buf[5];
    for (uint8_t c : b) { std::snprintf(buf, sizeof buf, "\\x%02X", c); s += buf; }
    return s + "'";
}

struct Conn {
    std::unique_ptr<duckdb::DuckDB> db;
    std::unique_ptr<duckdb::Connection> con;
    void open(const std::string& path) {
        std::filesystem::remove_all(path);
        db = std::make_unique<duckdb::DuckDB>(path);
        con = std::make_unique<duckdb::Connection>(*db);
    }
    duckdb::unique_ptr<duckdb::MaterializedQueryResult> q(const std::string& sql) {
        auto r = con->Query(sql);
        if (r->HasError()) throw std::runtime_error("duckdb: " + r->GetError());
        return duckdb::unique_ptr_cast<duckdb::QueryResult, duckdb::MaterializedQueryResult>(std::move(r));
    }
    void run(const std::string& sql) { auto m = q(sql); (void)m; }
};

// ---------------------------------------------------------------- archive R1
class DuckArchive : public ArchiveStore {
public:
    const char* engine() const override { return "duckdb"; }
    void open(const std::string& path, const OpenOpts&) override {
        c_.open(path);
        c_.run("CREATE TABLE blocks(num BIGINT, id BLOB, txns BIGINT, payload BLOB)");
        app_ = std::make_unique<duckdb::Appender>(*c_.con, "blocks");
    }
    void close() override { app_.reset(); c_.con.reset(); c_.db.reset(); }
    void put(const BlockRec& b) override {
        app_->AppendRow<int64_t, duckdb::Value, int64_t, duckdb::Value>(
            (int64_t)b.num, duckdb::Value::BLOB_RAW(S(b.id)),
            (int64_t)b.txns, duckdb::Value::BLOB_RAW(S(b.payload)));
    }
    void flush() override {
        app_->Close(); app_.reset();
        c_.run("CREATE INDEX idx_blocks_id ON blocks(id)");
    }
    bool getByNum(uint64_t num, Bytes& out) override {
        auto m = c_.q("SELECT payload FROM blocks WHERE num=" + std::to_string(num));
        if (m->RowCount() == 0) return false;
        out = B(m->GetValue(0, 0).ToString()); return true;
    }
    bool getById(const Bytes& id, Bytes& out) override {
        auto m = c_.q("SELECT payload FROM blocks WHERE id=" + blob(id));
        if (m->RowCount() == 0) return false;
        out = B(m->GetValue(0, 0).ToString()); return true;
    }
    void scanRange(uint64_t a, uint64_t b, bool asc, std::vector<uint64_t>& nums) override {
        auto m = c_.q("SELECT num FROM blocks WHERE num BETWEEN " + std::to_string(a) +
                       " AND " + std::to_string(b) + " ORDER BY num " + (asc ? "ASC" : "DESC"));
        for (idx_t i = 0; i < m->RowCount(); ++i) nums.push_back(m->GetValue(0, i).GetValue<int64_t>());
    }
    bool last(uint64_t& num) override { return agg("SELECT MAX(num) FROM blocks", num); }
    bool first(uint64_t& num) override { return agg("SELECT MIN(num) FROM blocks", num); }
    uint64_t count() override { uint64_t v = 0; agg("SELECT COUNT(*) FROM blocks", v); return v; }
    void analytics(uint64_t& txns, uint64_t& bytes) override {
        auto m = c_.q("SELECT SUM(txns)::BIGINT, SUM(octet_length(payload))::BIGINT FROM blocks");
        if (m->RowCount()) {
            txns = (uint64_t)m->GetValue(0, 0).GetValue<int64_t>();
            bytes = (uint64_t)m->GetValue(1, 0).GetValue<int64_t>();
        }
    }
    void snapshot(const std::string& to) override {
        std::filesystem::remove_all(to);
        c_.run("EXPORT DATABASE '" + to + "' (FORMAT PARQUET)");
    }

private:
    bool agg(const std::string& sql, uint64_t& v) {
        auto m = c_.q(sql);
        if (m->RowCount() == 0 || m->GetValue(0, 0).IsNull()) return false;
        v = (uint64_t)m->GetValue(0, 0).GetValue<int64_t>(); return true;
    }
    Conn c_;
    std::unique_ptr<duckdb::Appender> app_;
};

// ---------------------------------------------------------------- coin R2
class DuckCoin : public CoinStore {
public:
    const char* engine() const override { return "duckdb"; }
    void open(const std::string& path, const OpenOpts&) override {
        c_.open(path);
        c_.run("CREATE TABLE coins(coinid BLOB, addr BLOB, amount BIGINT, proof BLOB)");
        app_ = std::make_unique<duckdb::Appender>(*c_.con, "coins");
    }
    void close() override { app_.reset(); c_.con.reset(); c_.db.reset(); }
    void put(const CoinRec& c) override {
        app_->AppendRow<duckdb::Value, duckdb::Value, int64_t, duckdb::Value>(
            duckdb::Value::BLOB_RAW(S(c.coinid)), duckdb::Value::BLOB_RAW(S(c.address)),
            (int64_t)c.amount, duckdb::Value::BLOB_RAW(S(c.proof)));
    }
    void flush() override {
        app_->Close(); app_.reset();
        c_.run("CREATE INDEX idx_coins_addr ON coins(addr)");
    }
    bool get(const Bytes& coinid, Bytes& out) override {
        auto m = c_.q("SELECT proof FROM coins WHERE coinid=" + blob(coinid));
        if (m->RowCount() == 0) return false;
        out = B(m->GetValue(0, 0).ToString()); return true;
    }
    void prefixScan(const Bytes& addrPrefix, std::vector<Bytes>& coinids) override {
        Bytes hi = addrPrefix;
        for (size_t i = hi.size(); i-- > 0;) { if (++hi[i] != 0) break; }
        auto m = c_.q("SELECT coinid FROM coins WHERE addr>=" + blob(addrPrefix) + " AND addr<" + blob(hi));
        for (idx_t i = 0; i < m->RowCount(); ++i) coinids.push_back(B(m->GetValue(0, i).ToString()));
    }
    uint64_t count() override {
        auto m = c_.q("SELECT COUNT(*) FROM coins");
        return m->RowCount() ? (uint64_t)m->GetValue(0, 0).GetValue<int64_t>() : 0;
    }
    void snapshot(const std::string& to) override {
        std::filesystem::remove_all(to);
        c_.run("EXPORT DATABASE '" + to + "' (FORMAT PARQUET)");
    }

private:
    Conn c_;
    std::unique_ptr<duckdb::Appender> app_;
};

}  // namespace

std::unique_ptr<ArchiveStore> make_duckdb_archive() { return std::make_unique<DuckArchive>(); }
std::unique_ptr<CoinStore> make_duckdb_coin() { return std::make_unique<DuckCoin>(); }

}  // namespace eng

#else
namespace eng {
std::unique_ptr<ArchiveStore> make_duckdb_archive() { return nullptr; }
std::unique_ptr<CoinStore> make_duckdb_coin() { return nullptr; }
}  // namespace eng
#endif
