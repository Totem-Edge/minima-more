#include "org/minima/kissvm/functions/hex/b_i_t_c_o_u_n_t.hpp"

#include <cstddef>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"

#ifdef _WIN32
// No Windows-specific code required for this function currently.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

const int BITCOUNT::BITSPERBYTE[256] = {
    0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
};

BITCOUNT::BITCOUNT()
    : org::minima::kissvm::functions::MinimaFunction("BITCOUNT") {}

std::unique_ptr<org::minima::kissvm::values::Value>
BITCOUNT::runFunction(org::minima::kissvm::Contract& zContract) {
    checkExactParamNumber(requiredParams());

    // Get input HEX data
    auto hex = zContract.getHexParam(0, *this);
    auto data = hex->getRawData(); // std::vector<unsigned char>

    // Count set bits
    int bits = totalBits(data);

    // Return as NumberValue
    return std::make_unique<org::minima::kissvm::values::NumberValue>(bits);
}

int BITCOUNT::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction>
BITCOUNT::getNewFunction() {
    return std::make_unique<BITCOUNT>();
}

int BITCOUNT::totalBits(const std::vector<unsigned char>& zData) {
    int total = 0;
    for (size_t i = 0; i < zData.size(); ++i) {
        total += BITSPERBYTE[static_cast<unsigned int>(zData[i])];
    }
    return total;
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org