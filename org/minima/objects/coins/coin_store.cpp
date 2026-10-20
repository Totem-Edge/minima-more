#include "org/minima/objects/coins/coin_store.hpp"

#include <mutex>
#include <sstream>

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/base/mini_data.hpp"

#ifdef MINIMA_HAVE_LMDB
#include <lmdb.h>
#endif

namespace org {
namespace minima {
namespace objects {
namespace coins {

#ifdef MINIMA_HAVE_LMDB

namespace {

std::vector<std::uint8_t> serializeCoin(const Coin& coin) {
    auto md = org::minima::objects::base::MiniData::getMiniDataVersion(
        const_cast<Coin&>(coin));
    if (!md) return {};
    return md->getBytes();
}

std::unique_ptr<Coin> deserializeCoin(const void* data, std::size_t len) {
    std::string bytes(reinterpret_cast<const char*>(data), len);
    std::istringstream in(bytes, std::ios::binary);
    return Coin::ReadFromStream(in);
}

class LmdbCoinStore final : public CoinStore {
public:
    ~LmdbCoinStore() override { close(); }

    bool open(const std::filesystem::path& dir) override {
        close();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        if (mdb_env_create(&mEnv) != 0) return false;
        mdb_env_set_maxdbs(mEnv, 4);
        // 4 GiB of virtual address space; pages are touched on demand (mmap).
        mdb_env_set_mapsize(mEnv, static_cast<std::size_t>(4) * 1024 * 1024 * 1024);
        const int rc = mdb_env_open(mEnv, dir.string().c_str(), 0, 0600);
        if (rc != 0) {
            mdb_env_close(mEnv);
            mEnv = nullptr;
            return false;
        }

        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, 0, &txn) != 0) return false;
        const int drc = mdb_dbi_open(txn, "coins", MDB_CREATE, &mDbi);
        if (drc != 0) {
            mdb_txn_abort(txn);
            return false;
        }
        mdb_txn_commit(txn);
        mDir = dir;
        return true;
    }

    void close() override {
        if (mEnv) {
            mdb_env_close(mEnv);
            mEnv = nullptr;
            mDbi = 0;
        }
    }

    bool put(const Coin& coin) override { return putOne(coin); }

    bool putBatch(const std::vector<std::shared_ptr<Coin>>& coins) override {
        if (!mEnv) return false;
        if (coins.empty()) return true;
        std::lock_guard<std::mutex> lk(mWrite);

        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, 0, &txn) != 0) return false;

        for (const auto& c : coins) {
            if (!c) continue;
            const std::string key = c->getCoinID().to0xString();
            const std::vector<std::uint8_t> bytes = serializeCoin(*c);
            if (bytes.empty()) continue;

            MDB_val k{key.size(), const_cast<char*>(key.data())};
            MDB_val v{bytes.size(), const_cast<std::uint8_t*>(bytes.data())};
            if (mdb_put(txn, mDbi, &k, &v, 0) != 0) {
                mdb_txn_abort(txn);
                return false;
            }
        }
        return mdb_txn_commit(txn) == 0;
    }

    std::unique_ptr<Coin> get(const std::string& coinid) const override {
        if (!mEnv) return nullptr;
        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, MDB_RDONLY, &txn) != 0) return nullptr;

        MDB_val k{coinid.size(), const_cast<char*>(coinid.data())};
        MDB_val v{};
        std::unique_ptr<Coin> out;
        if (mdb_get(txn, mDbi, &k, &v) == 0) {
            out = deserializeCoin(v.mv_data, v.mv_size);
        }
        mdb_txn_abort(txn);
        return out;
    }

    bool erase(const std::string& coinid) override {
        if (!mEnv) return false;
        std::lock_guard<std::mutex> lk(mWrite);
        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, 0, &txn) != 0) return false;
        MDB_val k{coinid.size(), const_cast<char*>(coinid.data())};
        const int rc = mdb_del(txn, mDbi, &k, nullptr);
        mdb_txn_commit(txn);
        return rc == 0 || rc == MDB_NOTFOUND;
    }

    void forEach(const std::function<void(const Coin&)>& fn) const override {
        if (!mEnv) return;
        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, MDB_RDONLY, &txn) != 0) return;

        MDB_cursor* cur = nullptr;
        if (mdb_cursor_open(txn, mDbi, &cur) != 0) {
            mdb_txn_abort(txn);
            return;
        }

        MDB_val k{}, v{};
        int rc = mdb_cursor_get(cur, &k, &v, MDB_FIRST);
        while (rc == 0) {
            auto coin = deserializeCoin(v.mv_data, v.mv_size);
            if (coin) fn(*coin);
            rc = mdb_cursor_get(cur, &k, &v, MDB_NEXT);
        }
        mdb_cursor_close(cur);
        mdb_txn_abort(txn);
    }

    std::size_t size() const override {
        if (!mEnv) return 0;
        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, MDB_RDONLY, &txn) != 0) return 0;
        MDB_stat st{};
        std::size_t n = 0;
        if (mdb_stat(txn, mDbi, &st) == 0) n = st.ms_entries;
        mdb_txn_abort(txn);
        return n;
    }

    void clear() override {
        if (!mEnv) return;
        std::lock_guard<std::mutex> lk(mWrite);
        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, 0, &txn) != 0) return;
        mdb_drop(txn, mDbi, 0);
        mdb_txn_commit(txn);
    }

    void sync() override {
        if (mEnv) mdb_env_sync(mEnv, 1);
    }

    const char* engine() const override { return "lmdb"; }

private:
    bool putOne(const Coin& coin) {
        if (!mEnv) return false;
        std::lock_guard<std::mutex> lk(mWrite);
        MDB_txn* txn = nullptr;
        if (mdb_txn_begin(mEnv, nullptr, 0, &txn) != 0) return false;

        const std::string key = coin.getCoinID().to0xString();
        const std::vector<std::uint8_t> bytes = serializeCoin(coin);
        if (bytes.empty()) {
            mdb_txn_abort(txn);
            return false;
        }

        MDB_val k{key.size(), const_cast<char*>(key.data())};
        MDB_val v{bytes.size(), const_cast<std::uint8_t*>(bytes.data())};
        const int rc = mdb_put(txn, mDbi, &k, &v, 0);
        if (rc != 0) {
            mdb_txn_abort(txn);
            return false;
        }
        return mdb_txn_commit(txn) == 0;
    }

    MDB_env* mEnv = nullptr;
    MDB_dbi mDbi = 0;
    std::filesystem::path mDir;
    mutable std::mutex mWrite;
};

} // namespace

std::unique_ptr<CoinStore> createCoinStore() {
    return std::make_unique<LmdbCoinStore>();
}

#else

std::unique_ptr<CoinStore> createCoinStore() {
    return nullptr;  // fall back to MegaMMR's in-memory map
}

#endif

} // namespace coins
} // namespace objects
} // namespace minima
} // namespace org
