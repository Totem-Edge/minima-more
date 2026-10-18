#include "org/minima/kissvm/functions/hex/s_u_b_s_e_t.hpp"

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
namespace hex {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::Value;

SUBSET::SUBSET() : MinimaFunction("SUBSET") {}

std::unique_ptr<Value> SUBSET::runFunction(Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Get start and end indices
    int start = zContract.getNumberParam(0, *this)->getNumber().getAsInt();
    int end   = zContract.getNumberParam(1, *this)->getNumber().getAsInt();
    int len   = end - start;

    // Check size
    if (len < 0) {
        throw ExecutionException("Negative SUBSET length " + std::to_string(len));
    } else if (len > Contract::MAX_DATA_SIZE) {
        throw ExecutionException("SUBSET size too large " + std::to_string(len));
    }

    // Retrieve the original data
    std::vector<std::uint8_t> orig = zContract.getHexParam(2, *this)->getRawData();
    int origlen = static_cast<int>(orig.size());

    // Check limits
    if (start < 0 || start > origlen) {
        throw ExecutionException("SUBSET start outside size of data array " +
                                 std::to_string(start) + "-" + std::to_string(end) +
                                 " length:" + std::to_string(origlen));
    }

    if (end < 0 || end > origlen) {
        throw ExecutionException("SUBSET end outside size of data array " +
                                 std::to_string(start) + "-" + std::to_string(end) +
                                 " length:" + std::to_string(origlen));
    }

    // Extract subset
    std::vector<std::uint8_t> subs(static_cast<std::size_t>(len));
    if (len > 0) {
        std::copy(orig.begin() + start, orig.begin() + start + len, subs.begin());
    }

    return std::make_unique<HexValue>(subs);
}

int SUBSET::requiredParams() {
    return 3;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SUBSET::getNewFunction() {
    return std::make_unique<SUBSET>();
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org