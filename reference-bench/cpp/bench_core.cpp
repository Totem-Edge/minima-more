// bench_core.cpp - C++ mirror of reference-bench/java/BenchMinima.java metrics:
// WOTS public-key / sign / verify, SHA3-256 224B hashrate, MMR addEntry+getRoot.
// Performance only; see reference-bench/README.md (not a byte-parity oracle).
#include "org/minima/objects/keys/mash_wots_backend.hpp"
#include "org/minima/objects/keys/winternitz.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/mshs2/mshs2_mining.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

using org::minima::objects::keys::Winternitz;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using namespace org::minima::objects::mmr;

static double msSince(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(b - a).count();
}
static void metric(const char* n, double v, const char* u) { std::printf("%s\t%.6f\t%s\n", n, v, u); }

int main(int argc, char** argv) {
    const int wotsIters = (argc > 1) ? std::atoi(argv[1]) : 200;
    const int mmrN      = (argc > 2) ? std::atoi(argv[2]) : 20000;
    const int hashIters = (argc > 3) ? std::atoi(argv[3]) : 500000;

    org::minima::objects::keys::mash::installMashWotsBackend();

    const MiniData seed("0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
    const MiniData msg(std::vector<std::uint8_t>(32, 0));

    // warmup
    { Winternitz w(seed); volatile auto p = w.getPublicKey().getBytes().size(); (void)p; }

    // WOTS public key
    {
        auto t0 = std::chrono::steady_clock::now();
        MiniData pub;
        for (int i = 0; i < wotsIters; ++i) pub = Winternitz(seed).getPublicKey();
        auto t1 = std::chrono::steady_clock::now();
        metric("wots_pubkey_ms", msSince(t0, t1) / wotsIters, "ms/op");
    }

    Winternitz w(seed);
    const MiniData pub = w.getPublicKey();
    const MiniData sig = w.sign(msg);

    {
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < wotsIters; ++i) { volatile auto s = w.sign(msg).getBytes().size(); (void)s; }
        auto t1 = std::chrono::steady_clock::now();
        metric("wots_sign_ms", msSince(t0, t1) / wotsIters, "ms/op");
    }
    {
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < wotsIters; ++i) { volatile bool ok = Winternitz::verify(pub, msg, sig); (void)ok; }
        auto t1 = std::chrono::steady_clock::now();
        metric("wots_verify_ms", msSince(t0, t1) / wotsIters, "ms/op");
    }

    // SHA3-256 over a 224-byte header
    {
        std::vector<std::uint8_t> hdr(224, 0);
        std::uint8_t h[32];
        for (int i = 0; i < hashIters / 10; ++i) org::minima::utils::mshs2::mining::sha3Into(hdr, h);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < hashIters; ++i) org::minima::utils::mshs2::mining::sha3Into(hdr, h);
        auto t1 = std::chrono::steady_clock::now();
        const double s = msSince(t0, t1) / 1000.0;
        metric("sha3_224_hashes_per_sec", hashIters / s, "h/s");
    }

    // MMR
    {
        MMR mmr;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < mmrN; ++i) {
            std::vector<std::uint8_t> h(32, 0);
            h[0] = static_cast<std::uint8_t>(i & 0xFF);
            h[1] = static_cast<std::uint8_t>((i >> 8) & 0xFF);
            MMRData d(MiniData(h), MiniNumber::ZERO());
            mmr.addEntry(d);
        }
        auto root = mmr.getRoot();
        auto t1 = std::chrono::steady_clock::now();
        metric("mmr_add_entries_ms", msSince(t0, t1), "ms");
        metric("mmr_add_entries_per_sec", mmrN / (msSince(t0, t1) / 1000.0), "entries/s");
        if (!root) { std::printf("mmr_root\tNULL\n"); return 1; }
    }
    return 0;
}
