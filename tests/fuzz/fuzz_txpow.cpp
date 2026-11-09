/**
 * fuzz_txpow.cpp
 *
 * Fuzz target for TxPoW wire deserialization (IBD / P2P block surface).
 * TxPoW::readDataStream parses header + optional body and derives the TxPoW ID
 * and block weight, so this exercises the block-validation input path without
 * touching the database.
 */

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "org/minima/objects/tx_po_w.hpp"

using org::minima::objects::TxPoW;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 2 * 1024 * 1024) {
        return 0;
    }
    std::string input(reinterpret_cast<const char*>(data), size);
    std::istringstream iss(input, std::ios::binary);
    try {
        auto txp = TxPoW::ReadFromStream(iss);
        (void)txp;
    } catch (...) {
        // Expected for malformed input.
    }
    return 0;
}
