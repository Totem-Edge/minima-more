// bench_sha3.cpp - single-thread SHA3-256 over a 224-byte header, matching the
// Java harness metric sha3_224_hashes_per_sec (the mining inner-loop primitive).
#include "org/minima/utils/mshs2/mshs2_mining.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

int main(int argc, char** argv) {
    const int iters = (argc > 1) ? std::atoi(argv[1]) : 500000;
    std::vector<std::uint8_t> hdr(224, 0);
    std::uint8_t h[32];

    for (int i = 0; i < iters / 10; ++i) org::minima::utils::mshs2::mining::sha3Into(hdr, h);

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; ++i) org::minima::utils::mshs2::mining::sha3Into(hdr, h);
    const auto t1 = std::chrono::steady_clock::now();

    const double s = std::chrono::duration<double>(t1 - t0).count();
    std::printf("sha3_224_us_per_hash\t%.4f\tus/hash\n", s / iters * 1e6);
    std::printf("sha3_224_hashes_per_sec\t%.2f\th/s\n", iters / s);
    return 0;
}
