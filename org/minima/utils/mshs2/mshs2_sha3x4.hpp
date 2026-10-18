/**
 * mshs2_sha3x4.hpp
 *
 * General SHA3-256 x4 sponge over a portable x4 Keccak-f[1600] permutation
 * (ARMv8.2 SHA3 / NEON / scalar). Hashes four equal-length messages at once.
 * Used to batch the raw forest's variable-length SECRET (49 B) and fixed PUBLIC
 * (32 B) derivations (RFC-0002 7D.1/7D.2, productionized under RFC-0004).
 *
 * Byte-identical to single-lane SHA3-256 (validated 52/52 vs OpenSSL for
 * lengths 0..500). Falls back to a scalar tail for non-multiples of four.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "org/minima/utils/crypto.hpp"

#include <openssl/evp.h>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#define MSHS2_SHA3X4_NEON 1
#else
#define MSHS2_SHA3X4_NEON 0
#endif

#if MSHS2_SHA3X4_NEON && defined(__ARM_FEATURE_SHA3)
#define MSHS2_SHA3X4_ARM_SHA3 1
#else
#define MSHS2_SHA3X4_ARM_SHA3 0
#endif

namespace mshs2 {
namespace sha3x4 {

// RAII thread-local EVP context, freed at thread exit. The previous pattern
// (a bare `thread_local EVP_MD_CTX*` allocated with EVP_MD_CTX_new) leaked one
// context per thread.
struct EvpMdCtx {
    EVP_MD_CTX* ctx = nullptr;
    EvpMdCtx() { ctx = EVP_MD_CTX_new(); }
    ~EvpMdCtx() { if (ctx) EVP_MD_CTX_free(ctx); }
    EvpMdCtx(const EvpMdCtx&) = delete;
    EvpMdCtx& operator=(const EvpMdCtx&) = delete;
};

inline EVP_MD_CTX* tlsEvpMdCtx() {
    thread_local EvpMdCtx holder;
    return holder.ctx;
}

inline void scalarSha3(const std::uint8_t* in, std::size_t len, std::uint8_t out[32]) {
    EVP_MD_CTX* ctx = tlsEvpMdCtx();
    EVP_DigestInit_ex(ctx, org::minima::utils::Crypto::sha3Md(), nullptr);
    if (len) EVP_DigestUpdate(ctx, in, len);
    unsigned int o = 32;
    EVP_DigestFinal_ex(ctx, out, &o);
}

#if MSHS2_SHA3X4_NEON
namespace detail {

inline uint64x2_t rotl2(uint64x2_t x, int n) {
    return vorrq_u64(vshlq_u64(x, vdupq_n_s64(n)), vshlq_u64(x, vdupq_n_s64(-(64 - n))));
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

#if MSHS2_SHA3X4_ARM_SHA3
inline void keccakf1600x4Sha3(uint64x2_t a[25], uint64x2_t b[25]) {
    keccakf1600x4Impl(a, b,
        [](uint64x2_t x, uint64x2_t y, uint64x2_t z) { return veor3q_u64(x, y, z); },
        [](uint64x2_t x, uint64x2_t y, uint64x2_t z) { return vbcaxq_u64(x, y, z); });
}
#endif
inline void keccakf1600x4Neon(uint64x2_t a[25], uint64x2_t b[25]) {
    keccakf1600x4Impl(a, b, neonXor3, neonBca);
}

inline std::uint64_t loadLe64(const std::uint8_t* p) { std::uint64_t v; std::memcpy(&v, p, 8); return v; }
inline void storeLe64(std::uint8_t* p, std::uint64_t v) { std::memcpy(p, &v, 8); }

// General equal-length x4 sponge (SHA3-256, rate 136, suffix 0x06).
template <class KeccakX4>
inline void spongeEqual(KeccakX4 keccak,
                        const std::uint8_t* m0, const std::uint8_t* m1,
                        const std::uint8_t* m2, const std::uint8_t* m3, std::size_t len,
                        std::uint8_t* o0, std::uint8_t* o1, std::uint8_t* o2, std::uint8_t* o3) {
    constexpr std::size_t rate = 136;
    const std::uint8_t* msgs[4] = {m0, m1, m2, m3};
    uint64x2_t a[25], b[25];
    for (int i = 0; i < 25; ++i) { a[i] = vdupq_n_u64(0); b[i] = vdupq_n_u64(0); }
    const std::size_t nblocks = len / rate + 1;
    for (std::size_t bi = 0; bi < nblocks; ++bi) {
        const std::size_t base = bi * rate;
        const std::size_t rem = (len > base) ? (len - base) : 0;
        const std::size_t n = rem < rate ? rem : rate;
        const bool last = (bi == nblocks - 1);
        std::uint8_t blk[4][rate];
        for (int L = 0; L < 4; ++L) {
            std::memset(blk[L], 0, rate);
            if (n) std::memcpy(blk[L], msgs[L] + base, n);
            if (last) { blk[L][n] ^= 0x06; blk[L][rate - 1] ^= 0x80; }
        }
        for (std::size_t w = 0; w < rate / 8; ++w) {
            std::uint64_t t01[2] = {loadLe64(blk[0] + w * 8), loadLe64(blk[1] + w * 8)};
            std::uint64_t t23[2] = {loadLe64(blk[2] + w * 8), loadLe64(blk[3] + w * 8)};
            a[w] = veorq_u64(a[w], vld1q_u64(t01));
            b[w] = veorq_u64(b[w], vld1q_u64(t23));
        }
        keccak(a, b);
    }
    for (std::size_t w = 0; w < 4; ++w) {
        std::uint64_t t01[2], t23[2];
        vst1q_u64(t01, a[w]); vst1q_u64(t23, b[w]);
        storeLe64(o0 + w * 8, t01[0]); storeLe64(o1 + w * 8, t01[1]);
        storeLe64(o2 + w * 8, t23[0]); storeLe64(o3 + w * 8, t23[1]);
    }
}

} // namespace detail
#endif // MSHS2_SHA3X4_NEON

// SHA3-256 of four equal-length messages (x4), scalar fallback on non-NEON.
inline void sha3x4Equal(const std::uint8_t* m0, const std::uint8_t* m1,
                        const std::uint8_t* m2, const std::uint8_t* m3, std::size_t len,
                        std::uint8_t* o0, std::uint8_t* o1, std::uint8_t* o2, std::uint8_t* o3) {
#if MSHS2_SHA3X4_NEON
#if MSHS2_SHA3X4_ARM_SHA3
    detail::spongeEqual(detail::keccakf1600x4Sha3, m0, m1, m2, m3, len, o0, o1, o2, o3);
#else
    detail::spongeEqual(detail::keccakf1600x4Neon, m0, m1, m2, m3, len, o0, o1, o2, o3);
#endif
#else
    scalarSha3(m0, len, o0); scalarSha3(m1, len, o1);
    scalarSha3(m2, len, o2); scalarSha3(m3, len, o3);
#endif
}

} // namespace sha3x4
} // namespace mshs2
