#include "org/minima/kissvm/functions/cast/a_s_c_i_i.hpp"

#include <vector>
#include <cstdint>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

ASCII::ASCII()
    : org::minima::kissvm::functions::MinimaFunction("ASCII") {}

// Decode raw bytes as US-ASCII:
// - bytes 0x00..0x7F map directly to same code points
// - bytes 0x80..0xFF are unmappable and become Unicode replacement character U+FFFD (UTF-8: 0xEF 0xBF 0xBD)
std::unique_ptr<org::minima::kissvm::values::Value>
ASCII::runFunction(org::minima::kissvm::Contract& zContract) {
    checkExactParamNumber(requiredParams());

    // Get the HEX value
    std::unique_ptr<org::minima::kissvm::values::HexValue> hex =
        zContract.getHexParam(0, *this);

    // Convert to ASCII string with replacement for non-ASCII bytes
    const std::vector<std::uint8_t> data = hex->getRawData();
    std::string out;
    out.reserve(data.size()); // worst-case; replacement may grow, but reserve is just an optimization

    for (std::uint8_t b : data) {
        if (b <= 0x7Fu) {
            out.push_back(static_cast<char>(b));
        } else {
            // Append UTF-8 encoding of U+FFFD
            out.push_back(static_cast<char>(0xEF));
            out.push_back(static_cast<char>(0xBF));
            out.push_back(static_cast<char>(0xBD));
        }
    }

    return std::make_unique<org::minima::kissvm::values::StringValue>(out);
}

int ASCII::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> ASCII::getNewFunction() {
    return std::make_unique<ASCII>();
}

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org