// RFC-0003 — embedded storage-engine benchmark harness: common interface.
//
// One driver per engine implements ArchiveStore (role R1) and CoinStore (role R2)
// so the same workloads run against every candidate (see RFC-0001 §3, RFC-0003 §3).
// Keys/values are identical byte arrays across engines so no engine wins on codec.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eng {

using Bytes = std::vector<uint8_t>;

// R1: one record per TxBlock.
struct BlockRec {
    uint64_t num = 0;     // block height (also the archive key)
    Bytes id;             // 32-byte txpowid (secondary key)
    uint64_t txns = 0;    // txn count (for the analytics workload)
    Bytes payload;        // serialized TxBlock (identical across engines)
};

// R2: one record per UTXO.
struct CoinRec {
    Bytes coinid;         // 32-byte primary key
    Bytes address;        // 32-byte address hash (prefix-scan key)
    uint64_t amount = 0;
    Bytes proof;          // MMR entry + sibling path
};

struct OpenOpts {
    bool wal = true;      // durable-but-fast mode where applicable
    std::string extra;    // engine-specific hint (documented per driver)
};

class ArchiveStore {
public:
    virtual ~ArchiveStore() = default;
    virtual const char* engine() const = 0;
    virtual void open(const std::string& path, const OpenOpts& o) = 0;
    virtual void close() = 0;
    // bulk load: put() repeatedly, then flush() (engines may batch internally)
    virtual void put(const BlockRec& b) = 0;
    virtual void flush() = 0;
    virtual bool getByNum(uint64_t num, Bytes& out) = 0;
    virtual bool getById(const Bytes& id, Bytes& out) = 0;
    virtual void scanRange(uint64_t a, uint64_t b, bool asc,
                           std::vector<uint64_t>& nums) = 0;
    virtual bool last(uint64_t& num) = 0;
    virtual bool first(uint64_t& num) = 0;
    virtual uint64_t count() = 0;
    // derived analytics (R3 preview): total txns and total payload bytes
    virtual void analytics(uint64_t& txns, uint64_t& bytes) = 0;
    virtual void snapshot(const std::string& to) = 0;
};

class CoinStore {
public:
    virtual ~CoinStore() = default;
    virtual const char* engine() const = 0;
    virtual void open(const std::string& path, const OpenOpts& o) = 0;
    virtual void close() = 0;
    virtual void put(const CoinRec& c) = 0;
    virtual void flush() = 0;
    virtual bool get(const Bytes& coinid, Bytes& out) = 0;
    virtual void prefixScan(const Bytes& addrPrefix, std::vector<Bytes>& coinids) = 0;
    virtual uint64_t count() = 0;
    virtual void snapshot(const std::string& to) = 0;
};

// Factories. A driver returns nullptr when it is not compiled in (its library
// was absent at build time), so the runner simply skips it.
std::unique_ptr<ArchiveStore> make_sqlite_archive();
std::unique_ptr<CoinStore> make_sqlite_coin();
std::unique_ptr<ArchiveStore> make_sqlite_plain_archive();  // stock defaults, per-row
std::unique_ptr<CoinStore> make_sqlite_plain_coin();
std::unique_ptr<ArchiveStore> make_lmdb_archive();
std::unique_ptr<CoinStore> make_lmdb_coin();
std::unique_ptr<ArchiveStore> make_rocksdb_archive();
std::unique_ptr<CoinStore> make_rocksdb_coin();
std::unique_ptr<ArchiveStore> make_duckdb_archive();
std::unique_ptr<CoinStore> make_duckdb_coin();

}  // namespace eng
