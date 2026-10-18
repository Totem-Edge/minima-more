#ifndef ORG_MINIMA_OBJECTS_KEYS_WINTERNITZ_HPP
#define ORG_MINIMA_OBJECTS_KEYS_WINTERNITZ_HPP

#include <vector>
#include <array>
#include <cstdint>
#include <atomic>
#include <functional>
#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace objects {
namespace keys {

// Test/profiling-only counters. They record how many Winternitz objects are
// constructed and how many std::async tasks the WOTS path launches, so nested
// parallelism can be measured. They have no effect on cryptographic output.
struct WinternitzProfile {
    static std::atomic<std::uint64_t>& constructorCount() {
        static std::atomic<std::uint64_t> count{0};
        return count;
    }
    static std::atomic<std::uint64_t>& asyncTaskCount() {
        static std::atomic<std::uint64_t> count{0};
        return count;
    }
    static void reset() {
        constructorCount().store(0);
        asyncTaskCount().store(0);
    }
};

// Scheduling seam. Production leaves this empty and uses the legacy std::async
// path. When `run` is set, WOTS public-key chain hashing is scheduled through
// it instead. This changes ONLY scheduling: the sequential GMSSRandom seed
// expansion and the chain hash function are untouched, so output remains
// byte-identical.
//
// `run` MUST execute body(0..jobCount-1) and block until all complete. It MUST
// NOT be invoked from a worker of the same pool it submits to (callers keep the
// coordinator off the pool to avoid starvation).
struct WinternitzScheduler {
    std::function<void(std::size_t jobCount,
                       const std::function<void(std::size_t)>& body)> run;
    // Number of jobs to split the chains into. 0 => one job per chain.
    std::size_t jobCount = 0;
    // Informational: pool jobs submitted through this seam.
    std::atomic<std::uint64_t> poolJobsSubmitted{0};
};

// SIMD seam. When set, the batch hash of `count` independent 32-byte inputs is
// routed through it instead of the scalar loop. Only the hash execution
// changes; inputs, order, and outputs MUST be identical to the scalar
// `chain_hash_opt` results. GMSSRandom seed expansion is never routed here.
struct WinternitzSimdBackend {
    std::function<void(const std::uint8_t* inputs,
                       std::uint8_t* outputs,
                       std::size_t count)> hash32xN;
    // Informational: batched invocations and total lanes hashed.
    std::atomic<std::uint64_t> batchInvocations{0};
    std::atomic<std::uint64_t> lanesHashed{0};
};

// Process-wide seam accessors. Not thread-safe for concurrent set/use; the
// backend installs them once (std::call_once) before use.
WinternitzScheduler& winternitzScheduler();
WinternitzSimdBackend& winternitzSimdBackend();

// ARCHITECTURAL RULE: production Winternitz defaults to scalar + legacy async.
// A caller (the approved WOTS backend) may install a scheduler/SIMD seam once.

class Winternitz {
private:
    using Hash256 = std::array<std::uint8_t, 32>;
    
    org::minima::objects::base::MiniData mPrivateSeed;
    org::minima::objects::base::MiniData mPublicKey;
    
    // Private chains derived from the seed. Populated at construction and reused
    // by sign() (mutable so a default-constructed instance can lazily derive in
    // the const sign()). Deterministic from the seed.
    mutable std::vector<Hash256> mPrivateChains;

public:
    /**
     * Default constructor
     */
    Winternitz();
    
    /**
     * Create Winternitz signature scheme from private seed
     * @param zPrivateSeed The private seed (32 bytes)
     */
    Winternitz(const org::minima::objects::base::MiniData& zPrivateSeed);
    
    /**
     * Get the public key
     * @return The public key (32 bytes)
     */
    org::minima::objects::base::MiniData getPublicKey() const;
    
    /**
     * Sign data
     * @param zData The data to sign
     * @return The signature
     */
    org::minima::objects::base::MiniData sign(const org::minima::objects::base::MiniData& zData) const;
    
    /**
     * Verify a signature (static method)
     * @param zPublicKey The public key
     * @param zData The data that was signed
     * @param zSignature The signature
     * @return true if signature is valid, false otherwise
     */
    static bool verify(const org::minima::objects::base::MiniData& zPublicKey,
                      const org::minima::objects::base::MiniData& zData,
                      const org::minima::objects::base::MiniData& zSignature);
};

} // namespace keys
} // namespace objects
} // namespace minima
} // namespace org

#endif // ORG_MINIMA_OBJECTS_KEYS_WINTERNITZ_HPP