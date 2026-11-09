/**
 * fuzz_mini_data.cpp
 *
 * Fuzz target for MiniData deserialization (P2P message surface).
 *
 * With libFuzzer:  clang++ -fsanitize=fuzzer,address,undefined
 * Without it:      linked with fuzz_driver.cpp and run over a corpus:
 *                  fuzz_mini_data --fuzz 100000 seeds/*
 */

#include <cstddef>
#include <cstdint>
#include <sstream>

#include "org/minima/objects/base/mini_data.hpp"

using org::minima::objects::base::MiniData;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 16 * 1024 * 1024) {
        return 0;
    }
    std::string input(reinterpret_cast<const char*>(data), size);
    std::istringstream iss(input, std::ios::binary);
    try {
        MiniData md = MiniData::ReadFromStream(iss);
        (void)md;
    } catch (...) {
        // Expected for malformed input.
    }
    return 0;
}
