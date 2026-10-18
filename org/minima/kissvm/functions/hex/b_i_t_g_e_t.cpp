#include "org/minima/kissvm/functions/hex/b_i_t_g_e_t.hpp"

#include <cstddef>   // std::byte
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

BITGET::BITGET() : MinimaFunction("BITGET") {}

std::unique_ptr<org::minima::kissvm::values::Value>
BITGET::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact number of params
    checkExactParamNumber(requiredParams());

    // Get the input data (HEX as raw bytes)
    auto hexparam = zContract.getHexParam(0, *this);
    const auto& data = hexparam->getRawData();

    // Total bits available in the raw array
    int totbits = static_cast<int>(data.size() * 8) - 1;

    // Desired bit
    auto numparam = zContract.getNumberParam(1, *this);
    int bit = numparam->getNumber().getAsInt();

    if (bit < 0 || bit > totbits) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "BitGet too large " + std::to_string(bit) + " / " + std::to_string(totbits));
    }

    // Compute which byte and which bit within that byte
    const std::size_t byteIndex = static_cast<std::size_t>(bit) / 8;
    const int bitInByte = bit % 8;

    // Safely view underlying storage as unsigned char sequence
    const unsigned char* raw =
        reinterpret_cast<const unsigned char*>(data.data());

    const unsigned int ubyte = static_cast<unsigned int>(raw[byteIndex]);

    // Java BitSet.valueOf(byte[]) uses little-endian within each byte
    const bool isSet = ((ubyte >> bitInByte) & 0x1u) != 0;

    return std::make_unique<org::minima::kissvm::values::BooleanValue>(isSet);
}

std::unique_ptr<MinimaFunction> BITGET::getNewFunction() {
    return std::make_unique<BITGET>();
}

int BITGET::requiredParams() {
    return 2;
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org