#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace objects {

class Coin;

namespace coins {

// RFC-0001 R2: the UTXO/coin store. Serialized coins live in an embedded,
// mmap-backed KV store so the process does not hold the full coin map on the
// heap. LMDB is used when available; otherwise MegaMMR keeps its in-memory map.
class CoinStore {
public:
    virtual ~CoinStore() = default;

    virtual bool open(const std::filesystem::path& dir) = 0;
    virtual void close() = 0;

    virtual bool put(const Coin& coin) = 0;
    virtual bool putBatch(const std::vector<std::shared_ptr<Coin>>& coins) {
        for (const auto& c : coins) {
            if (!c || !put(*c)) return false;
        }
        return true;
    }

    virtual std::unique_ptr<Coin> get(const std::string& coinid) const = 0;
    virtual bool erase(const std::string& coinid) = 0;

    virtual void forEach(const std::function<void(const Coin&)>& fn) const = 0;

    // Iterate until fn returns false (early exit without materialising).
    virtual void forEachUntil(const std::function<bool(const Coin&)>& fn) const {
        bool cont = true;
        forEach([&](const Coin& c) { if (cont && !fn(c)) cont = false; });
    }

    virtual std::size_t size() const = 0;
    virtual bool empty() const { return size() == 0; }

    virtual void clear() = 0;
    virtual void sync() = 0;

    virtual const char* engine() const = 0;
};

// Returns an LMDB store when the build has MINIMA_HAVE_LMDB, else nullptr
// (callers fall back to the in-memory map).
std::unique_ptr<CoinStore> createCoinStore();

} // namespace coins
} // namespace objects
} // namespace minima
} // namespace org
