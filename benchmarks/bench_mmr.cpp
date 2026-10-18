// MMR peak/root caching benchmark
// Compares repeated getRoot()/getPeaks() calls against an MMR of configurable size.
//
// Usage: bench_mmr_new <entries> <iterations>
//
// The MMR is built once, then getRoot()/getPeaks() are called <iterations> times
// in a tight loop. This isolates the effect of peak/root caching.

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

using namespace org::minima;
using namespace org::minima::objects::mmr;
using namespace org::minima::objects::base;

static MiniData makeHash(size_t index) {
    // Deterministic 32-byte hash-like data
    std::vector<uint8_t> data(32, 0);
    for (size_t i = 0; i < 32; ++i) {
        data[i] = static_cast<uint8_t>((index * 31 + i * 7) % 256);
    }
    return MiniData(data);
}

int main(int argc, char** argv) {
    const size_t entries = (argc > 1) ? std::atoll(argv[1]) : 10000;
    const size_t iterations = (argc > 2) ? std::atoll(argv[2]) : 1000;

    std::cout << "MMR benchmark: " << entries << " entries, " << iterations << " root/peaks iterations\n";

    MMR mmr;

    // Build the MMR
    auto buildStart = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < entries; ++i) {
        MMRData data(makeHash(i), MiniNumber(static_cast<long long>(i)));
        mmr.addEntry(data);
    }
    auto buildEnd = std::chrono::high_resolution_clock::now();
    auto buildMs = std::chrono::duration_cast<std::chrono::milliseconds>(buildEnd - buildStart).count();
    std::cout << "Build time: " << buildMs << " ms\n";

    // Warm up caches in the new version; old version recomputes anyway.
    auto root0 = mmr.getRoot();
#ifdef HAS_PEAKS_REF
    auto peaks0 = mmr.getPeaksRef();
#else
    auto peaks0 = mmr.getPeaks();
#endif
    std::cout << "First root length: " << (root0 ? root0->getData().getLength() : 0) << " bytes, peaks: " << peaks0.size() << "\n";

    // Benchmark repeated getRoot()
    size_t rootAcc = 0;
    auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        auto r = mmr.getRoot();
        rootAcc += (r ? r->getData().getLength() : 0);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    double rootMs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() / 1e6;

    // Benchmark repeated getPeaks() / getPeaksRef()
    size_t peaksAcc = 0;
    auto t3 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
#ifdef HAS_PEAKS_REF
        const auto& p = mmr.getPeaksRef();
#else
        auto p = mmr.getPeaks();
#endif
        peaksAcc += p.size();
    }
    auto t4 = std::chrono::high_resolution_clock::now();
    double peaksMs = std::chrono::duration_cast<std::chrono::nanoseconds>(t4 - t3).count() / 1e6;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "getRoot() x" << iterations << ": " << rootMs << " ms (" << (rootMs / iterations) * 1000.0 << " us/call)\n";
    std::cout << "getPeaks() x" << iterations << ": " << peaksMs << " ms (" << (peaksMs / iterations) * 1000.0 << " us/call)\n";

    return 0;
}
