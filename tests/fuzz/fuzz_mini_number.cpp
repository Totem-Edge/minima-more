/**
 * fuzz_mini_number.cpp
 *
 * Fuzz target for the consensus-critical MiniNumber decimal parser and the
 * basic arithmetic/formatting surface used by block weight and amounts.
 */

#include <cstddef>
#include <cstdint>
#include <string>

#include "org/minima/objects/base/mini_number.hpp"

using org::minima::objects::base::MiniNumber;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 4096) {
        return 0;
    }
    const std::string input(reinterpret_cast<const char*>(data), size);
    try {
        MiniNumber a(input);
        MiniNumber b = MiniNumber::fromWideString(input);
        (void)a.compareTo(b);
        (void)a.add(b);
        (void)a.sub(b);
        (void)a.toString();
        (void)a.getAsBigInteger();
    } catch (...) {
        // Expected for malformed or out-of-range input.
    }
    return 0;
}
