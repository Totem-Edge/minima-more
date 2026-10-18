#include "org/minima/kissvm/functions/hex/r_e_v.hpp"

#include <cstdint>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/value.hpp"

#ifdef _WIN32
// No OS-specific logic required for this file.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

REV::REV()
    : org::minima::kissvm::functions::MinimaFunction("REV") {
}

std::unique_ptr<org::minima::kissvm::values::Value>
REV::runFunction(org::minima::kissvm::Contract& zContract) {
    checkExactParamNumber(requiredParams());

    // The Data (owning unique_ptr to HexValue)
    std::unique_ptr<org::minima::kissvm::values::HexValue> hex =
        zContract.getHexParam(0, *this);

    // Get the bytes
    std::vector<std::uint8_t> array = hex->getRawData();

    // Lengths
    const std::size_t datalen = array.size();

    // Create the reverse buffer
    std::vector<std::uint8_t> revdata(datalen);

    // Reverse
    for (std::size_t i = 0; i < datalen; ++i) {
        revdata[i] = array[datalen - i - 1];
    }

    // Return reversed value
    return std::make_unique<org::minima::kissvm::values::HexValue>(revdata);
}

int REV::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> REV::getNewFunction() {
    return std::make_unique<REV>();
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org