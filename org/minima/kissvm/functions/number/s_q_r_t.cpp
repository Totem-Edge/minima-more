#include "org/minima/kissvm/functions/number/s_q_r_t.hpp"

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No Windows-specific code needed; placeholder for potential platform differences.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

SQRT::SQRT() : org::minima::kissvm::functions::MinimaFunction("SQRT") {}

std::unique_ptr<org::minima::kissvm::values::Value>
SQRT::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Get first parameter as NumberValue
    std::unique_ptr<org::minima::kissvm::values::NumberValue> number =
        zContract.getNumberParam(0, *this);

    // Compute square root on the underlying MiniNumber
    org::minima::objects::base::MiniNumber result = number->getNumber().sqrt();

    // Return a new NumberValue with the result
    return std::make_unique<org::minima::kissvm::values::NumberValue>(result);
}

int SQRT::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SQRT::getNewFunction() {
    return std::make_unique<SQRT>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org