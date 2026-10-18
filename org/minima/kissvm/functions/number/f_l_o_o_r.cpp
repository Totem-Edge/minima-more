#include "org/minima/kissvm/functions/number/f_l_o_o_r.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No platform-specific code needed for this function
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

FLOOR::FLOOR() : org::minima::kissvm::functions::MinimaFunction("FLOOR") {}

std::unique_ptr<org::minima::kissvm::values::Value>
FLOOR::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact number of parameters
    checkExactParamNumber(requiredParams());

    // Get the parameter as a NumberValue
    std::unique_ptr<org::minima::kissvm::values::NumberValue> number =
        zContract.getNumberParam(0, *this);

    // Compute floor and return new NumberValue
    org::minima::objects::base::MiniNumber floored = number->getNumber().floor();
    return std::make_unique<org::minima::kissvm::values::NumberValue>(floored);
}

int FLOOR::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> FLOOR::getNewFunction() {
    return std::make_unique<FLOOR>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org