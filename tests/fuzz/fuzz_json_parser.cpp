/**
 * fuzz_json_parser.cpp
 *
 * Fuzz target for the JSON parser (RPC/network input surface).
 */

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"

using org::minima::utils::json::parser::JSONParser;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // minima-more's parser has no exported MAX_INPUT_SIZE; cap at 1 MB so the
    // fuzzer stays on realistic RPC payloads (hawk-node exposes the constant).
    if (size > 1024 * 1024) {
        return 0;
    }
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        JSONParser parser;
        parser.parse(input);
    } catch (...) {
        // Exceptions are expected for malformed input; the fuzzer is looking
        // for crashes, hangs, and sanitizer findings.
    }
    return 0;
}
