#include "org/minima/kissvm/functions/number/s_i_g_d_i_g.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::base::MiniNumber;

SIGDIG::SIGDIG()
    : org::minima::kissvm::functions::MinimaFunction("SIGDIG") {}

std::unique_ptr<Value> SIGDIG::runFunction(Contract& zContract) {
    // Ensure exact number of parameters
    checkExactParamNumber(requiredParams());

    // Get parameters
    std::unique_ptr<NumberValue> significantdigits = zContract.getNumberParam(0, *this);
    std::unique_ptr<NumberValue> number            = zContract.getNumberParam(1, *this);

    // Validate that precision is a whole number
    MiniNumber actnum = significantdigits->getNumber();
    if (!actnum.floor().isEqual(actnum)) {
        throw ExecutionException("SIGDIG precision must be to a whole Number");
    }

    // Validate that precision is non-negative
    if (significantdigits->getNumber().isLess(MiniNumber::ZERO())) {
        throw ExecutionException(
            "SIGDIG precision must be a positive whole number : " + significantdigits->toString());
    }

    // Apply significant digits and return result
    int prec = significantdigits->getNumber().getAsInt();
    MiniNumber res = number->getNumber().setSignificantDigits(prec);
    return std::make_unique<NumberValue>(res);
}

int SIGDIG::requiredParams() {
    return 2;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SIGDIG::getNewFunction() {
    return std::make_unique<SIGDIG>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org