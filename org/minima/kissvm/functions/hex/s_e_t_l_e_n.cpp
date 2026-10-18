#include "org/minima/kissvm/functions/hex/s_e_t_l_e_n.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::objects::base::MiniNumber;

SETLEN::SETLEN() : MinimaFunction("SETLEN") {}

std::unique_ptr<org::minima::kissvm::values::Value>
SETLEN::runFunction(Contract& zContract) {
    // Ensure the correct number of parameters
    checkExactParamNumber(requiredParams());

    // Desired length
    std::unique_ptr<NumberValue> num = zContract.getNumberParam(0, *this);
    int newlen = num->getNumber().getAsInt();

    if (newlen < 1 || newlen > Contract::MAX_DATA_SIZE) {
        throw ExecutionException(
            "SETLEN size MUST be > 0 and < " + std::to_string(Contract::MAX_DATA_SIZE) + " : " +
            std::to_string(newlen));
    }

    // Original data
    std::unique_ptr<HexValue> hv = zContract.getHexParam(1, *this);
    std::vector<std::uint8_t> orig = hv->getRawData();
    int origlen = static_cast<int>(orig.size());

    // Already correct size
    if (newlen == origlen) {
        return std::make_unique<HexValue>(orig);
    }

    // Create new array of the specified size (zero-initialized)
    std::vector<std::uint8_t> newarray(static_cast<std::size_t>(newlen), 0);

    // Copy into it as per Java logic
    if (origlen > newlen) {
        // Original is longer: take the last newlen bytes
        std::copy(orig.begin() + (origlen - newlen), orig.end(), newarray.begin());
    } else {
        // New array is longer: right-align original, left-pad with zeros
        std::copy(orig.begin(), orig.end(), newarray.begin() + (newlen - origlen));
    }

    return std::make_unique<HexValue>(newarray);
}

std::unique_ptr<MinimaFunction> SETLEN::getNewFunction() {
    return std::make_unique<SETLEN>();
}

int SETLEN::requiredParams() {
    return 2;
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org