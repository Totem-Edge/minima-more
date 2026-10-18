#include "org/minima/kissvm/functions/number/a_b_s.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

ABS::ABS() : MinimaFunction("ABS") {}

std::unique_ptr<org::minima::kissvm::values::Value>
ABS::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exactly the required number of parameters are provided
    checkExactParamNumber(requiredParams());

    // Retrieve the first parameter as a NumberValue
    std::unique_ptr<org::minima::kissvm::values::NumberValue> number =
        zContract.getNumberParam(0, *this);

    // Compute absolute value and return as a new NumberValue
    org::minima::objects::base::MiniNumber absnum = number->getNumber().abs();
    return std::make_unique<org::minima::kissvm::values::NumberValue>(absnum);
}

int ABS::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> ABS::getNewFunction() {
    return std::make_unique<ABS>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org