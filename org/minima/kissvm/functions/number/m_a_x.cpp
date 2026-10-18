#include "org/minima/kissvm/functions/number/m_a_x.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No OS-specific behavior required here, but guard retained per instruction.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

using org::minima::kissvm::Contract;
using org::minima::kissvm::expressions::Expression;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::base::MiniNumber;

MAX::MAX() : MinimaFunction("MAX") {}

std::unique_ptr<Value> MAX::runFunction(Contract& zContract) {
    // Ensure at least the required minimum number of parameters
    checkMinParamNumber(requiredParams());

    const auto& params = getAllParameters();

    bool first = true;
    MiniNumber maxnum; // Will be initialized on first iteration

    for (const auto& expr : params) {
        Value* numval = expr->getValue(zContract);
        // Ensure the value is numeric
        checkIsOfType(*numval, Value::VALUE_NUMBER);

        // Safe after type check
        NumberValue* chk = static_cast<NumberValue*>(numval);
        MiniNumber current = chk->getNumber();

        if (first) {
            first = false;
            maxnum = current;
        } else {
            if (current.isMore(maxnum)) {
                maxnum = current;
            }
        }
    }

    // Return a new NumberValue with the maximum numeric value
    return std::make_unique<NumberValue>(maxnum);
}

bool MAX::isRequiredMinimumParameterNumber() const {
    return true;
}

int MAX::requiredParams() {
    return 2;
}

std::unique_ptr<MinimaFunction> MAX::getNewFunction() {
    return std::make_unique<MAX>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org