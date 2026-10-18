#pragma once
//
// Self-contained TxPoW mining core (RFC-0006).
//
// Nonce encoding, the SHA3-256 difficulty predicate, and a bounded parallel
// nonce search. Deliberately node-independent: it depends only on the C++
// standard library and OpenSSL, so the same code backs the production node
// miner (org/minima/system/brains/tx_po_w_miner.cpp) and the standalone MASH
// research/bench harnesses. See RFC-0006 and
// tests/mshs2/bench/TXPOW_MINING_SCALING_INVESTIGATION.md.
//
// The SHA3-256 digest is the fetched provider EVP_MD (Crypto::sha3Md()); using
// the legacy EVP_sha3_256() placeholder forces a locked provider fetch on every
// call and caps multi-core scaling (measured ~1.8x vs ~4x on Ampere N1).

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <openssl/evp.h>

#include "org/minima/utils/crypto.hpp"

namespace org {
namespace minima {
namespace utils {
namespace mshs2 {
namespace mining {

// RAII wrapper for a thread-local OpenSSL EVP context.
struct EvpContextHolder {
    EVP_MD_CTX* ctx = nullptr;
    EvpContextHolder() {
        ctx = EVP_MD_CTX_new();
        if (!ctx) throw std::runtime_error("Failed to create EVP_MD_CTX");
    }
    ~EvpContextHolder() {
        if (ctx) {
            EVP_MD_CTX_free(ctx);
            ctx = nullptr;
        }
    }
    EvpContextHolder(const EvpContextHolder&) = delete;
    EvpContextHolder& operator=(const EvpContextHolder&) = delete;
    EvpContextHolder(EvpContextHolder&&) = delete;
    EvpContextHolder& operator=(EvpContextHolder&&) = delete;
};

inline EvpContextHolder& tlsHolder() {
    thread_local EvpContextHolder holder;
    return holder;
}

// Allocation-free SHA3-256 into a caller buffer (thread-local EVP).
inline void sha3Into(const std::uint8_t* data, std::size_t len, std::uint8_t out[32]) {
    EVP_MD_CTX* ctx = tlsHolder().ctx;
    if (EVP_DigestInit_ex(ctx, org::minima::utils::Crypto::sha3Md(), nullptr) != 1)
        throw std::runtime_error("EVP_DigestInit_ex failed");
    if (len && EVP_DigestUpdate(ctx, data, len) != 1)
        throw std::runtime_error("EVP_DigestUpdate failed");
    unsigned int outlen = 32;
    if (EVP_DigestFinal_ex(ctx, out, &outlen) != 1)
        throw std::runtime_error("EVP_DigestFinal_ex failed");
}

inline void sha3Into(const std::vector<std::uint8_t>& data, std::uint8_t out[32]) {
    sha3Into(data.data(), data.size(), out);
}

inline std::vector<std::uint8_t> sha3(const std::vector<std::uint8_t>& data) {
    std::vector<std::uint8_t> out(32);
    sha3Into(data, out.data());
    return out;
}

// Big-endian nonce, matching Java BigInteger.toByteArray() (0x00 if high bit set).
inline void encodeNonceBE(std::uint64_t v, std::vector<std::uint8_t>& out) {
    out.clear();
    if (v == 0) { out.push_back(0x00); return; }
    for (int shift = 56; shift >= 0; shift -= 8) {
        std::uint8_t b = static_cast<std::uint8_t>((v >> shift) & 0xFF);
        if (!out.empty() || b != 0) out.push_back(b);
    }
    if (out[0] & 0x80) out.insert(out.begin(), 0x00);
}

// Allocation-free equivalent of MiniData(hash).isLess(difficulty).
inline bool bytesLessThan(const std::uint8_t h[32], const std::uint8_t* d, std::size_t dlen) {
    std::size_t i = 0; while (i < 32 && h[i] == 0) ++i;
    std::size_t j = 0; while (j < dlen && d[j] == 0) ++j;
    const std::size_t lh = 32 - i, ld = dlen - j;
    if (lh != ld) return lh < ld;
    return std::memcmp(h + i, d + j, lh) < 0;
}

// Bounded parallel nonce search (RFC-0006 E-M2/E-M3). Workers share an atomic
// nonce counter and stop at the first satisfying nonce. Returns true and sets
// outNonce, or false if keepGoing() lapses first. Predicate/byte-equivalent to
// the sequential loop (any satisfying nonce is a valid TxPoW).
inline bool mineParallel(const std::vector<std::uint8_t>& base,
                         const std::vector<std::uint8_t>& diffBytes,
                         std::uint64_t& outNonce,
                         const std::function<bool()>& keepGoing,
                         std::size_t maxWorkers = 8) {
    unsigned hw = std::thread::hardware_concurrency();
    const std::size_t workers =
        std::max<std::size_t>(1, std::min<std::size_t>(hw ? hw : 1, maxWorkers));
    std::atomic<std::uint64_t> nextNonce{0};
    std::atomic<std::uint64_t> winner{0};
    std::atomic<bool> found{false};
    // R4: sha3Into can throw; a worker must not let it escape (std::terminate).
    // Capture the first exception, stop the search, join, then rethrow.
    std::mutex errMutex;
    std::exception_ptr err;

    auto scan = [&]() {
        try {
            std::vector<std::uint8_t> buf = base;
            std::vector<std::uint8_t> nb; nb.reserve(33);
            std::uint8_t h[32];
            while (!found.load(std::memory_order_relaxed)) {
                // Poll the stop condition cheaply (every 1024 nonces).
                if ((nextNonce.load(std::memory_order_relaxed) & 0x3FFu) == 0 && !keepGoing())
                    return;
                const std::uint64_t n = nextNonce.fetch_add(1, std::memory_order_relaxed);
                encodeNonceBE(n, nb);
                if (4 + nb.size() > buf.size()) continue;
                std::copy(nb.begin(), nb.end(), buf.begin() + 4);
                sha3Into(buf, h);
                if (bytesLessThan(h, diffBytes.data(), diffBytes.size())) {
                    if (!found.exchange(true, std::memory_order_relaxed))
                        winner.store(n, std::memory_order_relaxed);
                    return;
                }
            }
        } catch (...) {
            found.store(true, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lk(errMutex);
            if (!err) err = std::current_exception();
        }
    };

    if (workers <= 1) {
        scan();
    } else {
        std::vector<std::thread> pool;
        pool.reserve(workers);
        for (std::size_t w = 0; w < workers; ++w) pool.emplace_back(scan);
        for (auto& t : pool) t.join();
    }
    if (err) std::rethrow_exception(err);
    if (!found.load()) return false;
    outNonce = winner.load();
    return true;
}

} // namespace mining
} // namespace mshs2
} // namespace utils
} // namespace minima
} // namespace org
