#include "org/minima/kissvm/functions/number/p_o_w.hpp"

#include <memory>
#include <stdexcept>
#include <iostream>
#include <chrono>

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

POW::POW() : MinimaFunction("POW") {}

std::unique_ptr<Value> POW::runFunction(Contract& zContract) {
    // Check exact number of parameters
    checkExactParamNumber(requiredParams());

    // Get parameters as numbers
    std::unique_ptr<NumberValue> exp    = zContract.getNumberParam(0, *this);
    std::unique_ptr<NumberValue> number = zContract.getNumberParam(1, *this);

    MiniNumber actnum = exp->getNumber();

    // Ensure exponent is a whole number: floor(x) == x
    if (!actnum.floor().isEqual(actnum)) {
        throw ExecutionException("POW must be to a whole Number");
    }

    // Check exponent absolute value within limits (abs(exp) < 1024)
    if (actnum.abs().isMoreEqual(MiniNumber::THOUSAND24())) {
        throw ExecutionException("ABS POW exponent must be less than 1024");
    }

    // Compute integer power
    MiniNumber result = number->getNumber().pow(actnum.getAsInt());

    return std::make_unique<NumberValue>(result);
}

int POW::requiredParams() {
    return 2;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> POW::getNewFunction() {
    return std::make_unique<POW>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org

#ifdef MINIMA_POW_STANDALONE
int main(int argc, char* argv[]) {
    using org::minima::objects::base::MiniNumber;

    MiniNumber ww("0.01");

    auto timenow = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        MiniNumber pow = ww.pow(1000);
        std::cout << pow.toString() << std::endl;
    }
    auto timediff = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - timenow)
                        .count();

    std::cout << "Time : " << timediff << std::endl;
    return 0;
}
#endif