#include "org/minima/kissvm/functions/number/m_i_n.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"

#ifdef _WIN32
// No OS-specific logic needed for this file currently.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

MIN::MIN() : org::minima::kissvm::functions::MinimaFunction("MIN") {}

std::unique_ptr<org::minima::kissvm::values::Value> MIN::runFunction(org::minima::kissvm::Contract& zContract) {
    checkMinParamNumber(requiredParams());

    const auto& params = getAllParameters();

    bool first = true;
    const org::minima::kissvm::values::NumberValue* min = nullptr;

    for (const auto& p : params) {
        org::minima::kissvm::expressions::Expression* exp = p.get();
        org::minima::kissvm::values::Value* numval = exp->getValue(zContract);
        checkIsOfType(*numval, org::minima::kissvm::values::Value::VALUE_NUMBER);

        auto* chk = static_cast<org::minima::kissvm::values::NumberValue*>(numval);

        if (first) {
            first = false;
            min = chk;
        } else {
            if (chk->getNumber().isLess(min->getNumber())) {
                min = chk;
            }
        }
    }

    // Return a new NumberValue containing the minimum value found
    return std::make_unique<org::minima::kissvm::values::NumberValue>(min->getNumber());
}

bool MIN::isRequiredMinimumParameterNumber() const {
    return true;
}

int MIN::requiredParams() {
    return 2;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> MIN::getNewFunction() {
    return std::make_unique<MIN>();
}

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org