// RFC-0003 — RocksDB driver (role R1 archive + R2 coin store).
//
// LSM KV: two column families per store (data + a secondary index), bulk load via
// one WriteBatch. Snapshot via Checkpoint. Guarded by HAVE_ROCKSDB.
#include "interface.hpp"

#ifdef HAVE_ROCKSDB
#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/checkpoint.h>
#include <rocksdb/write_batch.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace eng {
namespace {

void be64(uint8_t* p, uint64_t v) { for (int i = 7; i >= 0; --i) { p[i] = v & 0xff; v >>= 8; } }
uint64_t rd64(const uint8_t* p) {
    uint64_t v = 0; for (int i = 0; i < 8; ++i) v = (v << 8) | p[i]; return v;
}

rocksdb::Options opts() {
    rocksdb::Options o;
    o.create_if_missing = true;
    o.create_missing_column_families = true;
    return o;
}

struct Db {
    std::unique_ptr<rocksdb::DB> db;
    std::vector<rocksdb::ColumnFamilyHandle*> cfs;
    void open(const std::string& path, const std::vector<std::string>& names) {
        std::filesystem::create_directories(path);
        std::vector<rocksdb::ColumnFamilyDescriptor> desc;
        desc.emplace_back(rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions());
        for (auto& n : names) desc.emplace_back(n, rocksdb::ColumnFamilyOptions());
        rocksdb::DB* raw = nullptr;
        auto st = rocksdb::DB::Open(opts(), path, desc, &cfs, &raw);
        db.reset(raw);
        if (!st.ok()) throw std::runtime_error("rocksdb open: " + st.ToString());
    }
    rocksdb::ColumnFamilyHandle* cf(size_t i) { return cfs[i + 1]; }  // skip default
    void close() {
        if (db) { for (auto* c : cfs) db->DestroyColumnFamilyHandle(c); }
        db.reset(); cfs.clear();
    }
};

std::string str(const uint8_t* p, size_t n) { return std::string(reinterpret_cast<const char*>(p), n); }

// ---------------------------------------------------------------- archive R1
class RocksArchive : public ArchiveStore {
public:
    const char* engine() const override { return "rocksdb"; }
    void open(const std::string& path, const OpenOpts&) override {
        db_.open(path, {"blocks", "ididx"});
        blocks_ = db_.cf(0); ididx_ = db_.cf(1);
    }
    void close() override { db_.close(); }
    void put(const BlockRec& b) override {
        uint8_t k[8]; be64(k, b.num);
        Bytes v(8 + b.payload.size()); be64(v.data(), b.txns);
        std::memcpy(v.data() + 8, b.payload.data(), b.payload.size());
        batch_.Put(blocks_, rocksdb::Slice(reinterpret_cast<char*>(k), 8), rocksdb::Slice(reinterpret_cast<char*>(v.data()), v.size()));
        batch_.Put(ididx_, rocksdb::Slice(reinterpret_cast<const char*>(b.id.data()), b.id.size()), rocksdb::Slice(reinterpret_cast<char*>(k), 8));
    }
    void flush() override { db_.db->Write(rocksdb::WriteOptions(), &batch_); batch_.Clear(); }
    bool getByNum(uint64_t num, Bytes& out) override {
        uint8_t k[8]; be64(k, num);
        std::string v;
        if (!db_.db->Get(rocksdb::ReadOptions(), blocks_, rocksdb::Slice(reinterpret_cast<char*>(k), 8), &v).ok()) return false;
        if (v.size() >= 8) out.assign(v.begin() + 8, v.end());
        return true;
    }
    bool getById(const Bytes& id, Bytes& out) override {
        std::string n;
        if (!db_.db->Get(rocksdb::ReadOptions(), ididx_, rocksdb::Slice(reinterpret_cast<const char*>(id.data()), id.size()), &n).ok() || n.size() < 8) return false;
        return getByNum(rd64(reinterpret_cast<const uint8_t*>(n.data())), out);
    }
    void scanRange(uint64_t a, uint64_t b, bool asc, std::vector<uint64_t>& nums) override {
        std::unique_ptr<rocksdb::Iterator> it(db_.db->NewIterator(rocksdb::ReadOptions(), blocks_));
        if (asc) {
            uint8_t ak[8]; be64(ak, a);
            for (it->Seek(rocksdb::Slice(reinterpret_cast<char*>(ak), 8)); it->Valid(); it->Next()) {
                uint64_t n = rd64(reinterpret_cast<const uint8_t*>(it->key().data()));
                if (n > b) break; nums.push_back(n);
            }
        } else {
            uint8_t bk[8]; be64(bk, b);
            it->SeekForPrev(rocksdb::Slice(reinterpret_cast<char*>(bk), 8));
            for (; it->Valid(); it->Prev()) {
                uint64_t n = rd64(reinterpret_cast<const uint8_t*>(it->key().data()));
                if (n < a) break; nums.push_back(n);
            }
        }
    }
    bool last(uint64_t& num) override {
        std::unique_ptr<rocksdb::Iterator> it(db_.db->NewIterator(rocksdb::ReadOptions(), blocks_));
        it->SeekToLast();
        if (!it->Valid()) return false;
        num = rd64(reinterpret_cast<const uint8_t*>(it->key().data())); return true;
    }
    bool first(uint64_t& num) override {
        std::unique_ptr<rocksdb::Iterator> it(db_.db->NewIterator(rocksdb::ReadOptions(), blocks_));
        it->SeekToFirst();
        if (!it->Valid()) return false;
        num = rd64(reinterpret_cast<const uint8_t*>(it->key().data())); return true;
    }
    uint64_t count() override {
        uint64_t n = 0;
        std::unique_ptr<rocksdb::Iterator> it(db_.db->NewIterator(rocksdb::ReadOptions(), blocks_));
        for (it->SeekToFirst(); it->Valid(); it->Next()) ++n;
        return n;
    }
    void analytics(uint64_t& txns, uint64_t& bytes) override {
        txns = bytes = 0;
        std::unique_ptr<rocksdb::Iterator> it(db_.db->NewIterator(rocksdb::ReadOptions(), blocks_));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            auto v = it->value();
            if (v.size() >= 8) { txns += rd64(reinterpret_cast<const uint8_t*>(v.data())); bytes += v.size() - 8; }
        }
    }
    void snapshot(const std::string& to) override {
        std::filesystem::remove_all(to);
        rocksdb::Checkpoint* cp = nullptr;
        rocksdb::Checkpoint::Create(db_.db.get(), &cp);
        cp->CreateCheckpoint(to); delete cp;
    }

private:
    Db db_;
    rocksdb::ColumnFamilyHandle *blocks_ = nullptr, *ididx_ = nullptr;
    rocksdb::WriteBatch batch_;
};

// ---------------------------------------------------------------- coin R2
class RocksCoin : public CoinStore {
public:
    const char* engine() const override { return "rocksdb"; }
    void open(const std::string& path, const OpenOpts&) override {
        db_.open(path, {"coins", "addridx"});
        coins_ = db_.cf(0); addridx_ = db_.cf(1);
    }
    void close() override { db_.close(); }
    void put(const CoinRec& c) override {
        Bytes v(8 + c.proof.size()); be64(v.data(), c.amount);
        std::memcpy(v.data() + 8, c.proof.data(), c.proof.size());
        batch_.Put(coins_, rocksdb::Slice(reinterpret_cast<const char*>(c.coinid.data()), c.coinid.size()), rocksdb::Slice(reinterpret_cast<char*>(v.data()), v.size()));
        Bytes ik = c.address; ik.insert(ik.end(), c.coinid.begin(), c.coinid.end());
        batch_.Put(addridx_, rocksdb::Slice(reinterpret_cast<char*>(ik.data()), ik.size()), rocksdb::Slice("", 0));
    }
    void flush() override { db_.db->Write(rocksdb::WriteOptions(), &batch_); batch_.Clear(); }
    bool get(const Bytes& coinid, Bytes& out) override {
        std::string v;
        if (!db_.db->Get(rocksdb::ReadOptions(), coins_, rocksdb::Slice(reinterpret_cast<const char*>(coinid.data()), coinid.size()), &v).ok()) return false;
        if (v.size() >= 8) out.assign(v.begin() + 8, v.end());
        return true;
    }
    void prefixScan(const Bytes& addrPrefix, std::vector<Bytes>& coinids) override {
        std::unique_ptr<rocksdb::Iterator> it(db_.db->NewIterator(rocksdb::ReadOptions(), addridx_));
        for (it->Seek(rocksdb::Slice(reinterpret_cast<const char*>(addrPrefix.data()), addrPrefix.size())); it->Valid(); it->Next()) {
            auto k = it->key();
            if (k.size() < addrPrefix.size() || std::memcmp(k.data(), addrPrefix.data(), addrPrefix.size()) != 0) break;
            coinids.emplace_back(reinterpret_cast<const uint8_t*>(k.data()) + addrPrefix.size(), reinterpret_cast<const uint8_t*>(k.data()) + k.size());
        }
    }
    uint64_t count() override {
        uint64_t n = 0;
        std::unique_ptr<rocksdb::Iterator> it(db_.db->NewIterator(rocksdb::ReadOptions(), coins_));
        for (it->SeekToFirst(); it->Valid(); it->Next()) ++n;
        return n;
    }
    void snapshot(const std::string& to) override {
        std::filesystem::remove_all(to);
        rocksdb::Checkpoint* cp = nullptr;
        rocksdb::Checkpoint::Create(db_.db.get(), &cp);
        cp->CreateCheckpoint(to); delete cp;
    }

private:
    Db db_;
    rocksdb::ColumnFamilyHandle *coins_ = nullptr, *addridx_ = nullptr;
    rocksdb::WriteBatch batch_;
};

}  // namespace

std::unique_ptr<ArchiveStore> make_rocksdb_archive() { return std::make_unique<RocksArchive>(); }
std::unique_ptr<CoinStore> make_rocksdb_coin() { return std::make_unique<RocksCoin>(); }

}  // namespace eng

#else
namespace eng {
std::unique_ptr<ArchiveStore> make_rocksdb_archive() { return nullptr; }
std::unique_ptr<CoinStore> make_rocksdb_coin() { return nullptr; }
}  // namespace eng
#endif
