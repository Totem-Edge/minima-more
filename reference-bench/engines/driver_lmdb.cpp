// RFC-0003 — LMDB driver (role R1 archive + R2 coin store).
//
// mmap B+tree: zero-copy point lookups, MVCC readers, one big bulk write txn.
// Schema is explicit (LMDB is KV, no SQL): a secondary index DBI provides the
// getById / prefix-scan paths, so it faces the same workloads as the row stores.
#include "interface.hpp"

#ifdef HAVE_LMDB
#include <lmdb.h>
#include <sys/stat.h>

#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace eng {
namespace {

constexpr size_t MAPSIZE = size_t(16) << 30;  // 16 GiB address space (sparse)

void be64(uint8_t* p, uint64_t v) { for (int i = 7; i >= 0; --i) { p[i] = v & 0xff; v >>= 8; } }
uint64_t rd64(const uint8_t* p) {
    uint64_t v = 0; for (int i = 0; i < 8; ++i) v = (v << 8) | p[i]; return v;
}
MDB_val val(const void* p, size_t n) { return MDB_val{n, const_cast<void*>(p)}; }

struct Env {
    MDB_env* env = nullptr;
    void open(const std::string& dir) {
        std::filesystem::create_directories(dir);
        if (mdb_env_create(&env)) throw std::runtime_error("mdb_env_create");
        mdb_env_set_mapsize(env, MAPSIZE);
        mdb_env_set_maxdbs(env, 8);
        if (mdb_env_open(env, dir.c_str(), 0, 0664)) throw std::runtime_error("mdb_env_open " + dir);
    }
    void close() { if (env) { mdb_env_close(env); env = nullptr; } }
};

// ---------------------------------------------------------------- archive R1
class LmdbArchive : public ArchiveStore {
public:
    const char* engine() const override { return "lmdb"; }
    void open(const std::string& path, const OpenOpts&) override {
        env_.open(path + "/archive");
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, 0, &t);
        mdb_dbi_open(t, "blocks", MDB_CREATE, &blocks_);
        mdb_dbi_open(t, "ididx", MDB_CREATE, &ididx_);
        mdb_txn_commit(t);
        mdb_txn_begin(env_.env, nullptr, 0, &bulk_);  // one bulk write txn
    }
    void close() override {
        if (bulk_) { mdb_txn_abort(bulk_); bulk_ = nullptr; }
        env_.close();
    }
    void put(const BlockRec& b) override {
        uint8_t k[8]; be64(k, b.num);
        Bytes v(8 + b.payload.size());
        be64(v.data(), b.txns);
        std::memcpy(v.data() + 8, b.payload.data(), b.payload.size());
        MDB_val vk = val(k, 8), vv = val(v.data(), v.size());
        mdb_put(bulk_, blocks_, &vk, &vv, 0);
        MDB_val ik = val(b.id.data(), b.id.size()), iv = val(k, 8);
        mdb_put(bulk_, ididx_, &ik, &iv, 0);
    }
    void flush() override { mdb_txn_commit(bulk_); bulk_ = nullptr; }
    bool getByNum(uint64_t num, Bytes& out) override {
        uint8_t k[8]; be64(k, num);
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_val key = val(k, 8), data;
        bool ok = mdb_get(t, blocks_, &key, &data) == 0;
        if (ok && data.mv_size >= 8)
            out.assign(static_cast<uint8_t*>(data.mv_data) + 8,
                       static_cast<uint8_t*>(data.mv_data) + data.mv_size);
        mdb_txn_abort(t);
        return ok;
    }
    bool getById(const Bytes& id, Bytes& out) override {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_val ik = val(id.data(), id.size()), data;
        bool ok = mdb_get(t, ididx_, &ik, &data) == 0;
        uint64_t num = ok ? rd64(static_cast<uint8_t*>(data.mv_data)) : 0;
        mdb_txn_abort(t);
        return ok && getByNum(num, out);
    }
    void scanRange(uint64_t a, uint64_t b, bool asc, std::vector<uint64_t>& nums) override {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_cursor* c; mdb_cursor_open(t, blocks_, &c);
        uint8_t ak[8], bk[8]; be64(ak, a); be64(bk, b);
        MDB_val key = val(asc ? ak : bk, 8), data;
        int rc = mdb_cursor_get(c, &key, &data, MDB_SET_RANGE);
        if (!asc && rc == 0 && rd64(static_cast<uint8_t*>(key.mv_data)) > b)
            rc = mdb_cursor_get(c, &key, &data, MDB_PREV);
        while (rc == 0) {
            uint64_t n = rd64(static_cast<uint8_t*>(key.mv_data));
            if (asc ? (n > b) : (n < a)) break;
            nums.push_back(n);
            rc = mdb_cursor_get(c, &key, &data, asc ? MDB_NEXT : MDB_PREV);
        }
        mdb_cursor_close(c);
        mdb_txn_abort(t);
    }
    bool last(uint64_t& num) override { return edge(num, MDB_LAST); }
    bool first(uint64_t& num) override { return edge(num, MDB_FIRST); }
    uint64_t count() override {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_stat st; mdb_stat(t, blocks_, &st);
        uint64_t n = st.ms_entries; mdb_txn_abort(t); return n;
    }
    void analytics(uint64_t& txns, uint64_t& bytes) override {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_cursor* c; mdb_cursor_open(t, blocks_, &c);
        MDB_val key, data;
        txns = bytes = 0;
        for (int rc = mdb_cursor_get(c, &key, &data, MDB_FIRST); rc == 0;
             rc = mdb_cursor_get(c, &key, &data, MDB_NEXT)) {
            if (data.mv_size >= 8) { txns += rd64(static_cast<uint8_t*>(data.mv_data));
                                     bytes += data.mv_size - 8; }
        }
        mdb_cursor_close(c); mdb_txn_abort(t);
    }
    void snapshot(const std::string& to) override { mdb_env_copy(env_.env, to.c_str()); }

private:
    bool edge(uint64_t& num, MDB_cursor_op op) {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_cursor* c; mdb_cursor_open(t, blocks_, &c);
        MDB_val key, data;
        bool ok = mdb_cursor_get(c, &key, &data, op) == 0;
        if (ok) num = rd64(static_cast<uint8_t*>(key.mv_data));
        mdb_cursor_close(c); mdb_txn_abort(t);
        return ok;
    }
    Env env_;
    MDB_dbi blocks_ = 0, ididx_ = 0;
    MDB_txn* bulk_ = nullptr;
};

// ---------------------------------------------------------------- coin R2
class LmdbCoin : public CoinStore {
public:
    const char* engine() const override { return "lmdb"; }
    void open(const std::string& path, const OpenOpts&) override {
        env_.open(path + "/coins");
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, 0, &t);
        mdb_dbi_open(t, "coins", MDB_CREATE, &coins_);
        mdb_dbi_open(t, "addridx", MDB_CREATE, &addridx_);
        mdb_txn_commit(t);
        mdb_txn_begin(env_.env, nullptr, 0, &bulk_);
    }
    void close() override {
        if (bulk_) { mdb_txn_abort(bulk_); bulk_ = nullptr; }
        env_.close();
    }
    void put(const CoinRec& c) override {
        Bytes v(8 + c.proof.size());
        be64(v.data(), c.amount);
        std::memcpy(v.data() + 8, c.proof.data(), c.proof.size());
        MDB_val ck = val(c.coinid.data(), c.coinid.size()), vv = val(v.data(), v.size());
        mdb_put(bulk_, coins_, &ck, &vv, 0);
        Bytes ik = c.address; ik.insert(ik.end(), c.coinid.begin(), c.coinid.end());
        MDB_val ak = val(ik.data(), ik.size()), empty = val("", 0);
        mdb_put(bulk_, addridx_, &ak, &empty, 0);
    }
    void flush() override { mdb_txn_commit(bulk_); bulk_ = nullptr; }
    bool get(const Bytes& coinid, Bytes& out) override {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_val key = val(coinid.data(), coinid.size()), data;
        bool ok = mdb_get(t, coins_, &key, &data) == 0;
        if (ok && data.mv_size >= 8)
            out.assign(static_cast<uint8_t*>(data.mv_data) + 8,
                       static_cast<uint8_t*>(data.mv_data) + data.mv_size);
        mdb_txn_abort(t);
        return ok;
    }
    void prefixScan(const Bytes& addrPrefix, std::vector<Bytes>& coinids) override {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_cursor* c; mdb_cursor_open(t, addridx_, &c);
        MDB_val key = val(addrPrefix.data(), addrPrefix.size()), data;
        for (int rc = mdb_cursor_get(c, &key, &data, MDB_SET_RANGE); rc == 0;
             rc = mdb_cursor_get(c, &key, &data, MDB_NEXT)) {
            const uint8_t* k = static_cast<const uint8_t*>(key.mv_data);
            if (key.mv_size < addrPrefix.size() ||
                std::memcmp(k, addrPrefix.data(), addrPrefix.size()) != 0) break;
            coinids.emplace_back(k + addrPrefix.size(), k + key.mv_size);
        }
        mdb_cursor_close(c); mdb_txn_abort(t);
    }
    uint64_t count() override {
        MDB_txn* t; mdb_txn_begin(env_.env, nullptr, MDB_RDONLY, &t);
        MDB_stat st; mdb_stat(t, coins_, &st);
        uint64_t n = st.ms_entries; mdb_txn_abort(t); return n;
    }
    void snapshot(const std::string& to) override { mdb_env_copy(env_.env, to.c_str()); }

private:
    Env env_;
    MDB_dbi coins_ = 0, addridx_ = 0;
    MDB_txn* bulk_ = nullptr;
};

}  // namespace

std::unique_ptr<ArchiveStore> make_lmdb_archive() { return std::make_unique<LmdbArchive>(); }
std::unique_ptr<CoinStore> make_lmdb_coin() { return std::make_unique<LmdbCoin>(); }

}  // namespace eng

#else  // no LMDB headers at build time

#include "interface.hpp"
namespace eng {
std::unique_ptr<ArchiveStore> make_lmdb_archive() { return nullptr; }
std::unique_ptr<CoinStore> make_lmdb_coin() { return nullptr; }
}  // namespace eng

#endif
