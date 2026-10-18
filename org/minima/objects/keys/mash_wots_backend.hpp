/**
 * mash_wots_backend.hpp  (RFC-0004 production substrate)
 *
 * Production SHA3-256 x4 WOTS backend + bounded pool, installed into the
 * Winternitz seams (winternitzSimdBackend / winternitzScheduler).
 *
 * Byte-identical to the scalar path: the x4 kernel is the validated RFC-0002
 * Experiment 6A/6B kernel (18/18 equivalence across 40,000 randomized lanes per
 * backend), and the pool only changes scheduling. The sequential GMSS seed
 * expansion is never routed here.
 *
 * The install is process-wide and idempotent (std::call_once). Per RFC-0004 the
 * Signer selects the approved backend; callers keep the coordinator thread off
 * the pool (the seam contract), which the Winternitz pooled paths satisfy.
 */

#pragma once

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <openssl/evp.h>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#define MASH_WOTS_HAVE_NEON 1
#else
#define MASH_WOTS_HAVE_NEON 0
#endif

#if MASH_WOTS_HAVE_NEON && defined(__ARM_FEATURE_SHA3)
#define MASH_WOTS_HAVE_ARM_SHA3 1
#else
#define MASH_WOTS_HAVE_ARM_SHA3 0
#endif

#include "org/minima/objects/keys/winternitz.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/mshs2/mshs2_sha3x4.hpp"

namespace org {
namespace minima {
namespace objects {
namespace keys {
namespace mash {

// ---- scalar SHA3-256 of 32 bytes (tail / fallback), matches chain_hash_opt ----
// Delegates to the RAII-backed thread-local implementation (no context leak).
inline void scalarSha3(const std::uint8_t* in, std::uint8_t* out) {
    mshs2::sha3x4::scalarSha3(in, 32, out);
}

#if MASH_WOTS_HAVE_NEON
namespace detail {

inline uint64x2_t rotl2(uint64x2_t x, int n) {
    return vorrq_u64(vshlq_u64(x, vdupq_n_s64(n)),
                     vshlq_u64(x, vdupq_n_s64(-(64 - n))));
}
inline uint64x2_t neonXor3(uint64x2_t a, uint64x2_t b, uint64x2_t c) {
    return veorq_u64(a, veorq_u64(b, c));
}
inline uint64x2_t neonBca(uint64x2_t a, uint64x2_t b, uint64x2_t c) {
    return veorq_u64(a, vbicq_u64(b, c));
}

template <class Xor3, class Bca>
inline void keccakf1600x4Impl(uint64x2_t a[25], uint64x2_t b[25], Xor3 xor3, Bca bca) {
    static const std::uint64_t RC[24] = {
        0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL,
        0x8000000080008000ULL, 0x000000000000808BULL, 0x0000000080000001ULL,
        0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008AULL,
        0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
        0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL,
        0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
        0x000000000000800AULL, 0x800000008000000AULL, 0x8000000080008081ULL,
        0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};
    static const int ROTC[24] = {1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
                                 27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44};
    static const int PILN[24] = {10, 7,  11, 17, 18, 3,  5,  16, 8,  21, 24, 4,
                                 15, 23, 19, 13, 12, 2,  20, 14, 22, 9,  6,  1};
    for (int r = 0; r < 24; ++r) {
        uint64x2_t ca[5], cb[5];
        for (int i = 0; i < 5; ++i) {
            ca[i] = veorq_u64(xor3(a[i], a[i + 5], a[i + 10]), veorq_u64(a[i + 15], a[i + 20]));
            cb[i] = veorq_u64(xor3(b[i], b[i + 5], b[i + 10]), veorq_u64(b[i + 15], b[i + 20]));
        }
        for (int i = 0; i < 5; ++i) {
            uint64x2_t da = veorq_u64(ca[(i + 4) % 5], rotl2(ca[(i + 1) % 5], 1));
            uint64x2_t db = veorq_u64(cb[(i + 4) % 5], rotl2(cb[(i + 1) % 5], 1));
            for (int j = 0; j < 25; j += 5) { a[j + i] = veorq_u64(a[j + i], da); b[j + i] = veorq_u64(b[j + i], db); }
        }
        {
            uint64x2_t ta = a[1], tb = b[1];
            for (int i = 0; i < 24; ++i) {
                int j = PILN[i];
                uint64x2_t tmpa = a[j], tmpb = b[j];
                a[j] = rotl2(ta, ROTC[i]); b[j] = rotl2(tb, ROTC[i]);
                ta = tmpa; tb = tmpb;
            }
        }
        for (int j = 0; j < 25; j += 5) {
            uint64x2_t ba[5], bb[5];
            for (int i = 0; i < 5; ++i) { ba[i] = a[j + i]; bb[i] = b[j + i]; }
            for (int i = 0; i < 5; ++i) {
                a[j + i] = bca(ba[i], ba[(i + 2) % 5], ba[(i + 1) % 5]);
                b[j + i] = bca(bb[i], bb[(i + 2) % 5], bb[(i + 1) % 5]);
            }
        }
        const uint64x2_t rc = vdupq_n_u64(RC[r]);
        a[0] = veorq_u64(a[0], rc); b[0] = veorq_u64(b[0], rc);
    }
}

#if MASH_WOTS_HAVE_ARM_SHA3
inline void keccakf1600x4Sha3(uint64x2_t a[25], uint64x2_t b[25]) {
    keccakf1600x4Impl(a, b,
        [](uint64x2_t x, uint64x2_t y, uint64x2_t z) { return veor3q_u64(x, y, z); },
        [](uint64x2_t x, uint64x2_t y, uint64x2_t z) { return vbcaxq_u64(x, y, z); });
}
#endif
inline void keccakf1600x4Neon(uint64x2_t a[25], uint64x2_t b[25]) {
    keccakf1600x4Impl(a, b, neonXor3, neonBca);
}

inline uint64x2_t loadLanePair(const uint8_t* p, const uint8_t* q, std::size_t w) {
    uint64_t t[2];
    std::memcpy(&t[0], p + w * 8, 8);
    std::memcpy(&t[1], q + w * 8, 8);
    return vld1q_u64(t);
}
inline void extractLanePair(uint64x2_t s[25], uint8_t* p, uint8_t* q) {
    for (std::size_t w = 0; w < 4; ++w) {
        uint64_t t[2];
        vst1q_u64(t, s[w]);
        std::memcpy(p + w * 8, &t[0], 8);
        std::memcpy(q + w * 8, &t[1], 8);
    }
}
template <class KeccakX4>
inline void hash32x4With(KeccakX4 keccak,
                         const uint8_t* i0, const uint8_t* i1, const uint8_t* i2, const uint8_t* i3,
                         uint8_t* o0, uint8_t* o1, uint8_t* o2, uint8_t* o3) {
    uint64x2_t sa[25], sb[25];
    for (int i = 0; i < 25; ++i) { sa[i] = vdupq_n_u64(0); sb[i] = vdupq_n_u64(0); }
    for (std::size_t w = 0; w < 4; ++w) {
        sa[w] = veorq_u64(sa[w], loadLanePair(i0, i1, w));
        sb[w] = veorq_u64(sb[w], loadLanePair(i2, i3, w));
    }
    const uint64x2_t dom = vdupq_n_u64(0x06ULL);
    const uint64x2_t fin = vdupq_n_u64(0x80ULL << 56);
    sa[4] = veorq_u64(sa[4], dom); sb[4] = veorq_u64(sb[4], dom);
    sa[16] = veorq_u64(sa[16], fin); sb[16] = veorq_u64(sb[16], fin);
    keccak(sa, sb);
    extractLanePair(sa, o0, o1);
    extractLanePair(sb, o2, o3);
}

} // namespace detail
#endif // MASH_WOTS_HAVE_NEON

// hash32xN over `count` independent 32-byte lanes (x4 + scalar tail).
inline void mashHash32xN(const std::uint8_t* in, std::uint8_t* out, std::size_t count) {
#if MASH_WOTS_HAVE_NEON
    std::size_t i = 0;
    for (; i + 4 <= count; i += 4) {
#if MASH_WOTS_HAVE_ARM_SHA3
        detail::hash32x4With(detail::keccakf1600x4Sha3, in + i * 32, in + (i + 1) * 32,
                             in + (i + 2) * 32, in + (i + 3) * 32,
                             out + i * 32, out + (i + 1) * 32, out + (i + 2) * 32, out + (i + 3) * 32);
#else
        detail::hash32x4With(detail::keccakf1600x4Neon, in + i * 32, in + (i + 1) * 32,
                             in + (i + 2) * 32, in + (i + 3) * 32,
                             out + i * 32, out + (i + 1) * 32, out + (i + 2) * 32, out + (i + 3) * 32);
#endif
    }
    for (; i < count; ++i) scalarSha3(in + i * 32, out + i * 32);
#else
    for (std::size_t i = 0; i < count; ++i) scalarSha3(in + i * 32, out + i * 32);
#endif
}

// ---- bounded worker pool for the WinternitzScheduler seam ----
class BoundedWotsPool {
public:
    explicit BoundedWotsPool(std::size_t workers) {
        if (workers == 0) workers = 1;
        for (std::size_t i = 0; i < workers; ++i)
            threads_.emplace_back([this] { workerLoop(); });
    }
    ~BoundedWotsPool() {
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : threads_) t.join();
    }
    BoundedWotsPool(const BoundedWotsPool&) = delete;
    BoundedWotsPool& operator=(const BoundedWotsPool&) = delete;

    // Execute body(0..jobCount-1) and block until all complete. Must be called
    // from a thread that is not one of this pool's workers (seam contract).
    void run(std::size_t jobCount, const std::function<void(std::size_t)>& body) {
        if (jobCount == 0) return;
        // R4: a body exception must not escape a worker thread (std::terminate).
        // Capture the first one in per-run state, always signal completion, then
        // rethrow on the coordinating thread after all jobs have drained.
        struct RunState {
            std::atomic<std::size_t> remaining;
            std::mutex errMutex;
            std::exception_ptr err;
            std::mutex done;
            std::condition_variable doneCv;
        };
        auto st = std::make_shared<RunState>();
        st->remaining.store(jobCount);
        {
            std::lock_guard<std::mutex> lk(m_);
            for (std::size_t i = 0; i < jobCount; ++i)
                queue_.push([&body, i, st] {
                    try {
                        body(i);
                    } catch (...) {
                        std::lock_guard<std::mutex> el(st->errMutex);
                        if (!st->err) st->err = std::current_exception();
                    }
                    if (st->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        std::lock_guard<std::mutex> dl(st->done);
                        st->doneCv.notify_all();
                    }
                });
        }
        cv_.notify_all();
        std::unique_lock<std::mutex> dl(st->done);
        st->doneCv.wait(dl, [&] { return st->remaining.load(std::memory_order_acquire) == 0; });
        dl.unlock();
        if (st->err) std::rethrow_exception(st->err);
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop();
            }
            task();
        }
    }
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> queue_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_ = false;
};

// Process-wide, idempotent install of the approved WOTS backend.
inline void installMashWotsBackend() {
    static std::once_flag once;
    std::call_once(once, [] {
        unsigned hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 1;
        // The WOTS chains are short; a little oversubscription hides per-job
        // latency (measured best at 8 workers on a 4-core A1 and an 8-core M1).
        std::size_t workers = std::min<std::size_t>(static_cast<std::size_t>(hw) * 2, 8);
        std::size_t jobs = 8;
        if (const char* e = std::getenv("MSHS2_WOTS_WORKERS")) {
            long v = std::strtol(e, nullptr, 10);
            if (v > 0) workers = static_cast<std::size_t>(v);
        }
        if (const char* e = std::getenv("MSHS2_WOTS_JOBS")) {
            long v = std::strtol(e, nullptr, 10);
            if (v > 0) jobs = static_cast<std::size_t>(v);
        }
        static BoundedWotsPool pool(workers);
        winternitzSimdBackend().hash32xN = &mashHash32xN;
        winternitzScheduler().jobCount = jobs;
        winternitzScheduler().run = [](std::size_t n,
                                       const std::function<void(std::size_t)>& b) { pool.run(n, b); };
    });
}

} // namespace mash
} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org
