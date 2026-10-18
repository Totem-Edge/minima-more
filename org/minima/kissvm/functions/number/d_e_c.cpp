#include "org/minima/kissvm/functions/number/d_e_c.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

#ifdef _WIN32
// No OS-specific behavior required for this class.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

DEC::DEC() : org::minima::kissvm::functions::MinimaFunction("DEC") {}

std::unique_ptr<org::minima::kissvm::values::Value>
DEC::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exactly the required number of parameters
    checkExactParamNumber(requiredParams());

    // Get the first parameter as a NumberValue
    std::unique_ptr<org::minima::kissvm::values::NumberValue> number =
        zContract.getNumberParam(0, *this);

    // Return a new NumberValue with decremented MiniNumber
    return std::make_unique<org::minima::kissvm::values::NumberValue>(
        number->getNumber().decrement());
}

int DEC::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> DEC::getNewFunction() {
    return std::make_unique<DEC>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org