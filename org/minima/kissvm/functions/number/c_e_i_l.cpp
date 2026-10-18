#include "org/minima/kissvm/functions/number/c_e_i_l.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

CEIL::CEIL()
    : org::minima::kissvm::functions::MinimaFunction("CEIL") {
}

std::unique_ptr<org::minima::kissvm::values::Value>
CEIL::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact number of parameters
    checkExactParamNumber(requiredParams());

    // Get the first parameter as a NumberValue
    std::unique_ptr<org::minima::kissvm::values::NumberValue> number =
        zContract.getNumberParam(0, *this);

    // Apply ceil on the underlying MiniNumber and return a new NumberValue
    return std::make_unique<org::minima::kissvm::values::NumberValue>(
        number->getNumber().ceil()
    );
}

int CEIL::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> CEIL::getNewFunction() {
    return std::make_unique<CEIL>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org