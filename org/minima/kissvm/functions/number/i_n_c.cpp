#include "org/minima/kissvm/functions/number/i_n_c.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

INC::INC() : org::minima::kissvm::functions::MinimaFunction("INC") {}

std::unique_ptr<org::minima::kissvm::values::Value>
INC::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Get the single numeric parameter
    std::unique_ptr<org::minima::kissvm::values::NumberValue> number =
        zContract.getNumberParam(0, *this);

    // Increment and return as a new NumberValue
    auto incremented =
        number->getNumber().increment();

    return std::make_unique<org::minima::kissvm::values::NumberValue>(incremented);
}

int INC::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> INC::getNewFunction() {
    return std::make_unique<INC>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org