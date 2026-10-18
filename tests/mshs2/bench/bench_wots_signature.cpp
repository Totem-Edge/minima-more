/**
 * bench_wots_signature.cpp
 *
 * Measures the WOTS signature chain hashing (Winternitz::sign) scalar vs the
 * batched+pooled path (the SIMD/scheduler seams installed by the MASH backend).
 * Also asserts the two are byte-identical.
 *
 * Build: cmake --build <build-dir> --target bench_wots_signature
 * Run:   ./bench_wots_signature [iters]
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/keys/mash_wots_backend.hpp"
#include "org/minima/objects/keys/winternitz.hpp"

using org::minima::objects::base::MiniData;
using org::minima::objects::keys::Winternitz;
using org::minima::objects::keys::winternitzScheduler;
using org::minima::objects::keys::winternitzSimdBackend;
using org::minima::objects::keys::mash::installMashWotsBackend;

static MiniData fill(int v, int n) { return MiniData(std::vector<std::uint8_t>(n, static_cast<std::uint8_t>(v))); }

static double msSince(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count() / 1000.0;
}

int main(int argc, char** argv) {
    const int iters = (argc > 1) ? std::atoi(argv[1]) : 50;
    std::cout << "=== WOTS signature chains: scalar vs batched+pooled ===\n";

    Winternitz w(fill(0x11, 32));
    const MiniData msg = fill(0xAB, 32);

    // Scalar: no seams.
    winternitzSimdBackend().hash32xN = nullptr;
    winternitzScheduler().run = nullptr;
    MiniData sigScalar;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; ++i) sigScalar = w.sign(msg);
    const auto t1 = std::chrono::steady_clock::now();

    // Batched + pooled: install the approved backend.
    installMashWotsBackend();
    MiniData sigBatched;
    const auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; ++i) sigBatched = w.sign(msg);
    const auto t3 = std::chrono::steady_clock::now();

    const double s = msSince(t0, t1), b = msSince(t2, t3);
    std::cout << "iters    " << iters << "\n";
    std::cout << "scalar   " << (s / iters) << " ms/sign  (" << s << " ms)\n";
    std::cout << "batched  " << (b / iters) << " ms/sign  (" << b << " ms)\n";
    std::cout << "speedup  " << (s / b) << "x\n";
    std::cout << "identical " << (sigScalar.to0xString() == sigBatched.to0xString() ? "yes" : "NO") << "\n";
    return (sigScalar.to0xString() == sigBatched.to0xString()) ? 0 : 1;
}
