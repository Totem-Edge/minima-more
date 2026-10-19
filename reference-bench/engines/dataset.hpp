// RFC-0003 — deterministic synthetic dataset, mainnet-shaped.
//
// Real `archive.raw.dat` / `mega.mmr` are the ideal source (RFC-0003 §4), but
// they live on the A1 lab box. This generator produces byte-identical records for
// every engine from a fixed seed, so the fair-comparison property (same data, same
// sizes, no codec wins) holds offline too. Swap in `load_from_archive()` later.
#pragma once

#include <cstdint>
#include <string>

#include "interface.hpp"

namespace eng {

struct Dataset {
    std::vector<BlockRec> blocks;
    std::vector<CoinRec> coins;
    uint64_t seed = 0;
};

// splitmix64: tiny, deterministic, no <random> engine overhead.
inline uint64_t sm64(uint64_t& x) {
    x += 0x9E3779B97F4A7C15ULL;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline void fillBytes(Bytes& b, size_t n, uint64_t& s) {
    b.resize(n);
    for (size_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(sm64(s) >> 24);
}

// Mainnet-shaped: ~2.4 M blocks, ~1.7 M coins, block payload ~600 B, proof ~200 B.
inline Dataset makeDataset(uint64_t nblocks, uint64_t ncoins, uint64_t seed) {
    Dataset d;
    d.seed = seed;
    d.blocks.reserve(nblocks);
    d.coins.reserve(ncoins);

    uint64_t s = seed;
    for (uint64_t i = 0; i < nblocks; ++i) {
        BlockRec b;
        b.num = i;
        fillBytes(b.id, 32, s);
        b.txns = 1 + (sm64(s) % 40);                 // 1..40 txns/block
        fillBytes(b.payload, 512 + (sm64(s) % 256), s);  // ~512..767 B
        d.blocks.push_back(std::move(b));
    }
    for (uint64_t i = 0; i < ncoins; ++i) {
        CoinRec c;
        fillBytes(c.coinid, 32, s);
        fillBytes(c.address, 32, s);
        c.amount = 1 + (sm64(s) % 1000000);
        fillBytes(c.proof, 160 + (sm64(s) % 96), s);  // ~160..255 B
        d.coins.push_back(std::move(c));
    }
    return d;
}

// Lookup probes: deterministic sample of keys (point + range), so every engine
// is queried with exactly the same keys.
inline std::vector<uint64_t> sampleNums(uint64_t nblocks, uint64_t k, uint64_t seed) {
    std::vector<uint64_t> v;
    uint64_t s = seed ^ 0xABCDEFULL;
    for (uint64_t i = 0; i < k && nblocks; ++i) v.push_back(sm64(s) % nblocks);
    return v;
}

inline std::vector<Bytes> sampleCoinIds(const std::vector<CoinRec>& coins, uint64_t k,
                                        uint64_t seed) {
    std::vector<Bytes> v;
    uint64_t s = seed ^ 0x123456ULL;
    for (uint64_t i = 0; i < k && !coins.empty(); ++i)
        v.push_back(coins[sm64(s) % coins.size()].coinid);
    return v;
}

}  // namespace eng
