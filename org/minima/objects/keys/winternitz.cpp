#include "org/minima/objects/keys/winternitz.hpp"

#include "org/minima/utils/crypto.hpp"

#include <cstring>
#include <algorithm>
#include <vector>
#include <array>
#include <future>
#include <thread>

// Use OpenSSL for optimized SHA3-256
#include <openssl/evp.h>

using org::minima::objects::base::MiniData;

namespace org {
namespace minima {
namespace objects {
namespace keys {

namespace {

// Fixed-size hash type for better performance
using Hash256 = std::array<std::uint8_t, 32>;

// Convert vector to fixed array
inline Hash256 to_hash256(const std::vector<std::uint8_t>& v) {
    Hash256 result;
    result.fill(0);
    std::copy_n(v.begin(), std::min(v.size(), size_t(32)), result.begin());
    return result;
}

// Optimized SHA3-256 using OpenSSL
inline Hash256 sha3_256_array(const Hash256& in) {
    Hash256 out;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_MD_CTX_reset(ctx);
    EVP_DigestInit_ex(ctx, org::minima::utils::Crypto::sha3Md(), nullptr);
    EVP_DigestUpdate(ctx, in.data(), 32);
    unsigned int len = 32;
    EVP_DigestFinal_ex(ctx, out.data(), &len);
    EVP_MD_CTX_free(ctx);
    return out;
}

inline std::vector<std::uint8_t> sha3_256_bytes(const std::vector<std::uint8_t>& in) {
    std::vector<std::uint8_t> out(32);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, org::minima::utils::Crypto::sha3Md(), nullptr);
    EVP_DigestUpdate(ctx, in.data(), in.size());
    unsigned int len = 32;
    EVP_DigestFinal_ex(ctx, out.data(), &len);
    EVP_MD_CTX_free(ctx);
    return out;
}

// Winternitz Parameters
constexpr int w_param() { return 8; }
constexpr int n_bytes() { return 32; }

inline int l1() {
    int n = n_bytes() * 8;
    int w = w_param();
    return (n + w - 1) / w;
}

// FIXED: Compute l2 to match BouncyCastle exactly
inline int getLog(int n) {
    int n2 = 1;
    int n3 = 2;
    while (n3 < n) {
        n3 <<= 1;
        ++n2;
    }
    return n2;
}

inline int l2() {
    int w = w_param();
    int messagesize = l1();
    int checksumsize = getLog((messagesize << w) + 1);
    return (checksumsize + w - 1) / w;
}

inline int l_total() {
    return l1() + l2();
}

/**
 * Derives private key chains using the BouncyCastle GMSSRandom.nextSeed()
 * semantics that Java Minima uses, so Java and C++ produce identical keys:
 *
 *   rand = H(seed)
 *   seed = seed + rand + 1  (mod 2^(8n))
 *   return rand
 *
 * Crucially, the byte-array addition treats index 0 as the least-significant
 * end (matching Bouncy Castle addByteArrays/addOne), NOT big-endian.
 *
 * RFC-0001 MSHS-2 section 2.3.
 */
std::vector<Hash256> derive_private_chains(const std::vector<std::uint8_t>& seed) {
    std::vector<Hash256> x;
    int total = l_total();
    x.reserve(total);

    // Normalize seed exactly like Java (zero-pad or truncate, NO HASHING)
    std::vector<std::uint8_t> current_seed(n_bytes(), 0);
    size_t copy_len = std::min(seed.size(), static_cast<size_t>(n_bytes()));
    std::copy_n(seed.begin(), copy_len, current_seed.begin());

    for (int i = 0; i < total; ++i) {
        // 1. rand = SHA3(current_seed)
        std::vector<std::uint8_t> rand = sha3_256_bytes(current_seed);
        x.push_back(to_hash256(rand));

        // 2. seed = seed + rand (index 0 = least significant)
        uint16_t carry = 0;
        for (size_t j = 0; j < current_seed.size(); ++j) {
            uint16_t v = static_cast<uint16_t>(current_seed[j]) +
                         static_cast<uint16_t>(rand[j]) + carry;
            current_seed[j] = static_cast<uint8_t>(v & 0xFF);
            carry = v >> 8;
        }

        // 3. seed = seed + 1 (index 0 = least significant)
        carry = 1;
        for (size_t j = 0; j < current_seed.size() && carry; ++j) {
            uint16_t v = static_cast<uint16_t>(current_seed[j]) + carry;
            current_seed[j] = static_cast<uint8_t>(v & 0xFF);
            carry = v >> 8;
        }
    }

    return x;
}

inline Hash256 chain_hash_opt(Hash256 current, int steps) {
    for (int i = 0; i < steps; ++i) {
        current = sha3_256_array(current);
    }
    return current;
}

// Legacy per-Winternitz std::async path (unchanged production behavior).
std::vector<std::uint8_t> compute_public_key_async(const std::vector<Hash256>& x) {
    const int max_steps = (1 << w_param()) - 1;
    const int num_chains = l_total();

    const int num_threads = std::min(std::thread::hardware_concurrency(),
                                     static_cast<unsigned int>(num_chains));

    std::vector<Hash256> results(num_chains);
    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    int chains_per_thread = (num_chains + num_threads - 1) / num_threads;

    for (int t = 0; t < num_threads; ++t) {
        int start_idx = t * chains_per_thread;
        int end_idx = std::min(start_idx + chains_per_thread, num_chains);

        if (start_idx >= num_chains) break;

        WinternitzProfile::asyncTaskCount().fetch_add(1, std::memory_order_relaxed);
        futures.push_back(std::async(std::launch::async,
            [&x, &results, start_idx, end_idx]() {
                for (int i = start_idx; i < end_idx; ++i) {
                    results[i] = chain_hash_opt(x[i], max_steps);
                }
            }
        ));
    }

    for (auto& fut : futures) {
        fut.get();
    }

    std::vector<std::uint8_t> concat;
    concat.reserve(num_chains * n_bytes());
    for (const auto& hash : results) {
        concat.insert(concat.end(), hash.begin(), hash.end());
    }

    return sha3_256_bytes(concat);
}

// Experimental pooled path (RFC-0002 Experiment 5). Schedules the same
// chain_hash_opt work through a caller-provided bounded executor. Only
// scheduling differs; the hash function is identical.
std::vector<std::uint8_t> compute_public_key_pooled(const std::vector<Hash256>& x,
                                                    WinternitzScheduler& sched) {
    const int max_steps = (1 << w_param()) - 1;
    const int num_chains = l_total();

    std::size_t jobCount = sched.jobCount;
    if (jobCount == 0 || jobCount > static_cast<std::size_t>(num_chains)) {
        jobCount = static_cast<std::size_t>(num_chains);
    }

    std::vector<Hash256> results(num_chains);
    sched.poolJobsSubmitted.fetch_add(jobCount, std::memory_order_relaxed);
    sched.run(jobCount, [&](std::size_t jobIndex) {
        const std::size_t per = static_cast<std::size_t>(num_chains) / jobCount;
        const std::size_t extra = static_cast<std::size_t>(num_chains) % jobCount;
        std::size_t start = jobIndex * per + std::min(jobIndex, extra);
        std::size_t count = per + (jobIndex < extra ? 1u : 0u);
        for (std::size_t i = start; i < start + count; ++i) {
            results[i] = chain_hash_opt(x[i], max_steps);
        }
    });

    std::vector<std::uint8_t> concat;
    concat.reserve(num_chains * n_bytes());
    for (const auto& hash : results) {
        concat.insert(concat.end(), hash.begin(), hash.end());
    }

    return sha3_256_bytes(concat);
}

// Experimental batched public-key path (RFC-0002 Experiment 6A). All 34 chains
// are driven to their terminal public value in lockstep, so each round hashes 34
// independent 32-byte values. When `hash32xN` is installed, a vectorized SHA3
// can hash `count` lanes at once; otherwise a scalar per-lane lockstep runs.
// The GMSS seed derivation that produced `x` stays sequential and untouched,
// and results are identical to the scalar `chain_hash_opt` path.
std::vector<std::uint8_t> compute_public_key_batched(const std::vector<Hash256>& x) {
    const int max_steps = (1 << w_param()) - 1;
    const int num_chains = l_total();

    std::vector<std::uint8_t> cur(static_cast<std::size_t>(num_chains) * n_bytes(), 0);
    for (int i = 0; i < num_chains; ++i) {
        std::copy(x[i].begin(), x[i].end(),
                  cur.begin() + static_cast<std::size_t>(i) * n_bytes());
    }

    WinternitzSimdBackend& simd = winternitzSimdBackend();
    std::vector<std::uint8_t> next(cur.size());
    for (int step = 0; step < max_steps; ++step) {
        if (simd.hash32xN) {
            simd.batchInvocations.fetch_add(1, std::memory_order_relaxed);
            simd.lanesHashed.fetch_add(static_cast<std::uint64_t>(num_chains),
                                       std::memory_order_relaxed);
            simd.hash32xN(cur.data(), next.data(),
                          static_cast<std::size_t>(num_chains));
        } else {
            for (int i = 0; i < num_chains; ++i) {
                Hash256 in;
                std::copy_n(cur.begin() + static_cast<std::size_t>(i) * n_bytes(),
                            n_bytes(), in.begin());
                Hash256 out = sha3_256_array(in);
                std::copy(out.begin(), out.end(),
                          next.begin() + static_cast<std::size_t>(i) * n_bytes());
            }
        }
        cur.swap(next);
    }

    std::vector<std::uint8_t> concat;
    concat.reserve(static_cast<std::size_t>(num_chains) * n_bytes());
    for (int i = 0; i < num_chains; ++i) {
        concat.insert(concat.end(),
                      cur.begin() + static_cast<std::size_t>(i) * n_bytes(),
                      cur.begin() + static_cast<std::size_t>(i + 1) * n_bytes());
    }
    return sha3_256_bytes(concat);
}

// Hashing of a contiguous [start, start+count) chain range in lockstep, using
// the installed SIMD backend (or scalar lanes). Shared by the single-thread and
// pooled-batched paths so composition reuses one primitive.
void batchedRange(const std::vector<Hash256>& x, std::size_t start, std::size_t count,
                  WinternitzSimdBackend& simd, std::uint8_t* out) {
    const int max_steps = (1 << w_param()) - 1;
    std::vector<std::uint8_t> cur(count * n_bytes(), 0);
    for (std::size_t i = 0; i < count; ++i) {
        std::copy(x[start + i].begin(), x[start + i].end(),
                  cur.begin() + i * n_bytes());
    }
    std::vector<std::uint8_t> next(cur.size());
    for (int step = 0; step < max_steps; ++step) {
        if (simd.hash32xN) {
            simd.batchInvocations.fetch_add(1, std::memory_order_relaxed);
            simd.lanesHashed.fetch_add(static_cast<std::uint64_t>(count),
                                       std::memory_order_relaxed);
            simd.hash32xN(cur.data(), next.data(), count);
        } else {
            for (std::size_t i = 0; i < count; ++i) {
                Hash256 in;
                std::copy_n(cur.begin() + i * n_bytes(), n_bytes(), in.begin());
                Hash256 o = sha3_256_array(in);
                std::copy(o.begin(), o.end(), next.begin() + i * n_bytes());
            }
        }
        cur.swap(next);
    }
    std::copy(cur.begin(), cur.end(), out);
}

// Pooled + batched composition (RFC-0002 Experiment 6B): the bounded scheduler
// splits the 34 chains into jobs; each job runs the thread-free batched lockstep
// over its own range. SIMD never manages threads; the scheduler never touches
// individual hashes. Both seams stay orthogonal.
std::vector<std::uint8_t> compute_public_key_pooled_batched(
        const std::vector<Hash256>& x, WinternitzScheduler& sched,
        WinternitzSimdBackend& simd) {
    const int num_chains = l_total();
    std::size_t jobCount = sched.jobCount;
    if (jobCount == 0 || jobCount > static_cast<std::size_t>(num_chains)) {
        jobCount = static_cast<std::size_t>(num_chains);
    }

    std::vector<std::uint8_t> out(static_cast<std::size_t>(num_chains) * n_bytes(), 0);
    sched.poolJobsSubmitted.fetch_add(jobCount, std::memory_order_relaxed);
    sched.run(jobCount, [&](std::size_t jobIndex) {
        const std::size_t per = static_cast<std::size_t>(num_chains) / jobCount;
        const std::size_t extra = static_cast<std::size_t>(num_chains) % jobCount;
        std::size_t start = jobIndex * per + std::min(jobIndex, extra);
        std::size_t count = per + (jobIndex < extra ? 1u : 0u);
        batchedRange(x, start, count, simd, out.data() + start * n_bytes());
    });

    std::vector<std::uint8_t> concat;
    concat.reserve(static_cast<std::size_t>(num_chains) * n_bytes());
    for (int i = 0; i < num_chains; ++i) {
        concat.insert(concat.end(),
                      out.begin() + static_cast<std::size_t>(i) * n_bytes(),
                      out.begin() + static_cast<std::size_t>(i + 1) * n_bytes());
    }
    return sha3_256_bytes(concat);
}

std::vector<std::uint8_t> compute_public_key(const std::vector<Hash256>& x) {
    WinternitzSimdBackend& simd = winternitzSimdBackend();
    WinternitzScheduler& sched = winternitzScheduler();
    if (simd.hash32xN && sched.run) {
        return compute_public_key_pooled_batched(x, sched, simd);
    }
    if (simd.hash32xN) {
        return compute_public_key_batched(x);
    }
    if (sched.run) {
        return compute_public_key_pooled(x, sched);
    }
    return compute_public_key_async(x);
}

// ---------------------------------------------------------------------------
// Batched signature chains (the signing counterpart of compute_public_key).
// Each chain i must be hashed steps[i] times (variable, unlike the public-key
// lockstep). Lanes are processed in ascending step order, so the still-active
// lanes form a contiguous suffix and the x4 kernel hashes them with no
// gather/scatter. Byte-identical to chain_hash_opt per lane.
// ---------------------------------------------------------------------------
void batchedSignatureRange(const std::vector<Hash256>& x, const std::vector<int>& steps,
                           std::size_t start, std::size_t count, WinternitzSimdBackend& simd,
                           std::uint8_t* out) {
    std::vector<std::size_t> idx(count);
    for (std::size_t k = 0; k < count; ++k) idx[k] = k;
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        return steps[start + a] < steps[start + b];
    });

    std::vector<std::uint8_t> cur(count * n_bytes(), 0);
    std::vector<std::uint8_t> next(cur.size());
    for (std::size_t k = 0; k < count; ++k) {
        std::copy(x[start + idx[k]].begin(), x[start + idx[k]].end(),
                  cur.begin() + k * n_bytes());
    }

    // Lanes needing zero steps are already final.
    std::size_t p = 0;
    while (p < count && steps[start + idx[p]] <= 0) ++p;

    int done = 0;
    while (p < count) {
        const std::size_t n = count - p;
        if (simd.hash32xN) {
            simd.batchInvocations.fetch_add(1, std::memory_order_relaxed);
            simd.lanesHashed.fetch_add(static_cast<std::uint64_t>(n), std::memory_order_relaxed);
            simd.hash32xN(cur.data() + p * n_bytes(), next.data() + p * n_bytes(), n);
            std::copy(next.begin() + p * n_bytes(), next.end(), cur.begin() + p * n_bytes());
        } else {
            for (std::size_t k = p; k < count; ++k) {
                Hash256 in;
                std::copy_n(cur.begin() + k * n_bytes(), n_bytes(), in.begin());
                Hash256 o = sha3_256_array(in);
                std::copy(o.begin(), o.end(), cur.begin() + k * n_bytes());
            }
        }
        ++done;
        while (p < count && steps[start + idx[p]] <= done) ++p;
    }

    for (std::size_t k = 0; k < count; ++k) {
        std::copy(cur.begin() + k * n_bytes(), cur.begin() + (k + 1) * n_bytes(),
                  out + (start + idx[k]) * n_bytes());
    }
}

std::vector<std::uint8_t> compute_signature_scalar(const std::vector<Hash256>& x,
                                                   const std::vector<int>& steps) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(l_total()) * n_bytes());
    for (int i = 0; i < l_total(); ++i) {
        Hash256 v = chain_hash_opt(x[i], steps[i]);
        std::copy(v.begin(), v.end(), out.begin() + static_cast<std::size_t>(i) * n_bytes());
    }
    return out;
}

std::vector<std::uint8_t> compute_signature_batched(const std::vector<Hash256>& x,
                                                    const std::vector<int>& steps,
                                                    WinternitzSimdBackend& simd) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(l_total()) * n_bytes(), 0);
    batchedSignatureRange(x, steps, 0, static_cast<std::size_t>(l_total()), simd, out.data());
    return out;
}

std::vector<std::uint8_t> compute_signature_pooled(const std::vector<Hash256>& x,
                                                   const std::vector<int>& steps,
                                                   WinternitzScheduler& sched) {
    const int num_chains = l_total();
    std::size_t jobCount = sched.jobCount;
    if (jobCount == 0 || jobCount > static_cast<std::size_t>(num_chains)) {
        jobCount = static_cast<std::size_t>(num_chains);
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(num_chains) * n_bytes(), 0);
    sched.poolJobsSubmitted.fetch_add(jobCount, std::memory_order_relaxed);
    sched.run(jobCount, [&](std::size_t jobIndex) {
        const std::size_t per = static_cast<std::size_t>(num_chains) / jobCount;
        const std::size_t extra = static_cast<std::size_t>(num_chains) % jobCount;
        const std::size_t start = jobIndex * per + std::min(jobIndex, extra);
        const std::size_t count = per + (jobIndex < extra ? 1u : 0u);
        for (std::size_t i = start; i < start + count; ++i) {
            Hash256 v = chain_hash_opt(x[i], steps[i]);
            std::copy(v.begin(), v.end(), out.begin() + i * n_bytes());
        }
    });
    return out;
}

std::vector<std::uint8_t> compute_signature_pooled_batched(const std::vector<Hash256>& x,
                                                           const std::vector<int>& steps,
                                                           WinternitzScheduler& sched,
                                                           WinternitzSimdBackend& simd) {
    const int num_chains = l_total();
    std::size_t jobCount = sched.jobCount;
    if (jobCount == 0 || jobCount > static_cast<std::size_t>(num_chains)) {
        jobCount = static_cast<std::size_t>(num_chains);
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(num_chains) * n_bytes(), 0);
    sched.poolJobsSubmitted.fetch_add(jobCount, std::memory_order_relaxed);
    sched.run(jobCount, [&](std::size_t jobIndex) {
        const std::size_t per = static_cast<std::size_t>(num_chains) / jobCount;
        const std::size_t extra = static_cast<std::size_t>(num_chains) % jobCount;
        const std::size_t start = jobIndex * per + std::min(jobIndex, extra);
        const std::size_t count = per + (jobIndex < extra ? 1u : 0u);
        if (count) batchedSignatureRange(x, steps, start, count, simd, out.data());
    });
    return out;
}

std::vector<std::uint8_t> compute_signature(const std::vector<Hash256>& x,
                                            const std::vector<int>& steps) {
    WinternitzSimdBackend& simd = winternitzSimdBackend();
    WinternitzScheduler& sched = winternitzScheduler();
    if (simd.hash32xN && sched.run) {
        return compute_signature_pooled_batched(x, steps, sched, simd);
    }
    if (simd.hash32xN) {
        return compute_signature_batched(x, steps, simd);
    }
    if (sched.run) {
        return compute_signature_pooled(x, steps, sched);
    }
    return compute_signature_scalar(x, steps);
}

} // anonymous namespace

WinternitzScheduler& winternitzScheduler() {
    static WinternitzScheduler instance;
    return instance;
}

WinternitzSimdBackend& winternitzSimdBackend() {
    static WinternitzSimdBackend instance;
    return instance;
}

Winternitz::Winternitz() = default;

Winternitz::Winternitz(const MiniData& zPrivateSeed)
: mPrivateSeed(zPrivateSeed) {
    WinternitzProfile::constructorCount().fetch_add(1, std::memory_order_relaxed);
    const std::vector<std::uint8_t>& seed = mPrivateSeed.getBytes();

    // Derive (and cache) the private chains once. They are deterministic from
    // the seed, so reusing them in sign() is byte-identical to re-deriving.
    mPrivateChains = derive_private_chains(seed);
    std::vector<std::uint8_t> pub = compute_public_key(mPrivateChains);

    mPublicKey = MiniData(pub);
}

MiniData Winternitz::getPublicKey() const {
    return mPublicKey;
}

MiniData Winternitz::sign(const MiniData& zData) const {
    const std::vector<std::uint8_t>& data = zData.getBytes();

    // Reuse the chains cached at construction. Derive lazily only for a
    // default-constructed instance (deterministic either way).
    if (mPrivateChains.empty()) {
        mPrivateChains = derive_private_chains(mPrivateSeed.getBytes());
    }
    const std::vector<Hash256>& x = mPrivateChains;

    std::vector<std::uint8_t> hashed = sha3_256_bytes(data);
    
    // Convert to base-w - exactly like BouncyCastle for w=8
    int w = w_param();
    int messagesize = l1();
    std::vector<int> msg;
    msg.reserve(messagesize);
    
    // FIXED: Use BouncyCastle's exact logic for w=8 (8 % w == 0 case)
    if (8 % w == 0) {
        int n = 8 / w;  // For w=8, n=1 (one value per byte)
        int mask = (1 << w) - 1;  // 255 for w=8
        int sum = 0;
        
        for (int i = 0; i < static_cast<int>(hashed.size()); ++i) {
            for (int j = 0; j < n; ++j) {
                int val = hashed[i] & mask;
                sum += val;
                msg.push_back(val);
                hashed[i] = (std::uint8_t)(hashed[i] >> w);
            }
        }
        
        // FIXED: Calculate checksum using BouncyCastle formula
        int checksum = (messagesize << w) - sum;
        
        // Convert checksum to base-w
        int checksumsize = getLog((messagesize << w) + 1);
        for (int i = 0; i < checksumsize; i += w) {
            int val = checksum & mask;
            msg.push_back(val);
            checksum >>= w;
        }
    } else {
        // For other w values, use the general case
        // (This branch is not used for w=8, but kept for completeness)
        int bits_per_digit = w;
        int mask = (1 << bits_per_digit) - 1;
        
        for (int i = 0; i < messagesize; ++i) {
            int byte_idx = (i * bits_per_digit) / 8;
            int bit_offset = (i * bits_per_digit) % 8;
            
            if (byte_idx < static_cast<int>(hashed.size())) {
                int val = hashed[byte_idx] >> bit_offset;
                if (bit_offset + bits_per_digit > 8 && byte_idx + 1 < static_cast<int>(hashed.size())) {
                    val |= (hashed[byte_idx + 1] << (8 - bit_offset));
                }
                msg.push_back(val & mask);
            } else {
                msg.push_back(0);
            }
        }
        
        int sum = 0;
        for (int val : msg) {
            sum += val;
        }
        int checksum = (messagesize << w) - sum;
        
        int l2_val = l2();
        for (int i = 0; i < l2_val; ++i) {
            msg.push_back(checksum & mask);
            checksum >>= bits_per_digit;
        }
    }
    
    // Generate signature. Batched/pooled when the SIMD/scheduler seams are
    // installed (same dispatch as public-key construction); scalar otherwise.
    return MiniData(compute_signature(x, msg));
}

bool Winternitz::verify(const MiniData& zPublicKey, const MiniData& zData, const MiniData& zSignature) {
    const std::vector<std::uint8_t>& data = zData.getBytes();
    const std::vector<std::uint8_t>& signature = zSignature.getBytes();
    
    if (signature.size() != static_cast<size_t>(l_total() * n_bytes())) {
        return false;
    }
    
    std::vector<std::uint8_t> hashed = sha3_256_bytes(data);
    
    // Convert to base-w - exactly like BouncyCastle for w=8
    int w = w_param();
    int messagesize = l1();
    std::vector<int> msg;
    msg.reserve(messagesize);
    
    // FIXED: Use BouncyCastle's exact logic for w=8 (8 % w == 0 case)
    if (8 % w == 0) {
        int n = 8 / w;  // For w=8, n=1 (one value per byte)
        int mask = (1 << w) - 1;  // 255 for w=8
        int sum = 0;
        
        for (int i = 0; i < static_cast<int>(hashed.size()); ++i) {
            for (int j = 0; j < n; ++j) {
                int val = hashed[i] & mask;
                sum += val;
                msg.push_back(val);
                hashed[i] = (std::uint8_t)(hashed[i] >> w);
            }
        }
        
        // FIXED: Calculate checksum using BouncyCastle formula
        int checksum = (messagesize << w) - sum;
        
        // Convert checksum to base-w
        int checksumsize = getLog((messagesize << w) + 1);
        for (int i = 0; i < checksumsize; i += w) {
            int val = checksum & mask;
            msg.push_back(val);
            checksum >>= w;
        }
    } else {
        // For other w values, use the general case
        int bits_per_digit = w;
        int mask = (1 << bits_per_digit) - 1;
        
        for (int i = 0; i < messagesize; ++i) {
            int byte_idx = (i * bits_per_digit) / 8;
            int bit_offset = (i * bits_per_digit) % 8;
            
            if (byte_idx < static_cast<int>(hashed.size())) {
                int val = hashed[byte_idx] >> bit_offset;
                if (bit_offset + bits_per_digit > 8 && byte_idx + 1 < static_cast<int>(hashed.size())) {
                    val |= (hashed[byte_idx + 1] << (8 - bit_offset));
                }
                msg.push_back(val & mask);
            } else {
                msg.push_back(0);
            }
        }
        
        int sum = 0;
        for (int val : msg) {
            sum += val;
        }
        int checksum = (messagesize << w) - sum;
        
        int l2_val = l2();
        for (int i = 0; i < l2_val; ++i) {
            msg.push_back(checksum & mask);
            checksum >>= bits_per_digit;
        }
    }
    
    // Verify (batched/pooled when the seams are installed).
    const int max_val = (1 << w) - 1;
    std::vector<Hash256> sigparts(l_total());
    std::vector<int> remaining(l_total());
    for (int i = 0; i < l_total(); ++i) {
        const int sig_offset = i * n_bytes();
        if (static_cast<std::size_t>(sig_offset) + n_bytes() > signature.size()) {
            return false;
        }
        std::copy_n(signature.begin() + sig_offset, n_bytes(), sigparts[i].begin());
        remaining[i] = max_val - msg[i];
    }
    std::vector<std::uint8_t> derived_pubkey = compute_signature(sigparts, remaining);
    std::vector<std::uint8_t> computed_pubkey = sha3_256_bytes(derived_pubkey);
    
    return (CRYPTO_memcmp(computed_pubkey.data(), zPublicKey.getBytes().data(), 32) == 0);
}

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org
