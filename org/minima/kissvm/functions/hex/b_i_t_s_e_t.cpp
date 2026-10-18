#include "org/minima/kissvm/functions/hex/b_i_t_s_e_t.hpp"

#include <algorithm>
#include <cstdint>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/value.hpp"

#ifdef _WIN32
// No OS-specific behavior required; placeholder for potential future differences.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::Value;

BITSET::BITSET() : org::minima::kissvm::functions::MinimaFunction("BITSET") {}

std::unique_ptr<Value> BITSET::runFunction(Contract& zContract) {
    // Ensure correct parameter count
    checkExactParamNumber(requiredParams());

    // Get the Input Data
    std::vector<std::uint8_t> data = zContract.getHexParam(0, *this)->getRawData();
    int totbits = static_cast<int>(data.size()) * 8 - 1;

    // Get the desired Bit
    int bit = zContract.getNumberParam(1, *this)->getNumber().getAsInt();
    if (bit < 0 || bit > totbits) {
        throw ExecutionException("BitSet too large " + std::to_string(bit) + " / " + std::to_string(totbits));
    }

    // Set to ON or OFF
    bool set = zContract.getBoolParam(2, *this)->isTrue();

    // Change the bit (BitSet semantics: bit 0 is LSB of byte 0)
    const std::size_t byte_index = static_cast<std::size_t>(bit) / 8;
    const std::size_t bit_index  = static_cast<std::size_t>(bit) % 8;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit_index);

    if (set) {
        data[byte_index] = static_cast<std::uint8_t>(data[byte_index] | mask);
    } else {
        data[byte_index] = static_cast<std::uint8_t>(data[byte_index] & static_cast<std::uint8_t>(~mask));
    }

    // Emulate BitSet.toByteArray(): trim trailing zero bytes (highest indices)
    std::size_t newlen = data.size();
    while (newlen > 0 && data[newlen - 1] == 0) {
        --newlen;
    }
    std::vector<std::uint8_t> newarray;
    newarray.assign(data.begin(), data.begin() + newlen);

    // Return the new HEXValue
    return std::make_unique<HexValue>(newarray);
}

int BITSET::requiredParams() {
    return 3;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> BITSET::getNewFunction() {
    return std::make_unique<BITSET>();
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org