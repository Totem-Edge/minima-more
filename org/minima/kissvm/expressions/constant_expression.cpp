#include "org/minima/kissvm/expressions/constant_expression.hpp"

#include <utility>
#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

ConstantExpression::ConstantExpression(std::unique_ptr<org::minima::kissvm::values::Value> zValue)
    : mValue(std::move(zValue)) {}

ConstantExpression::~ConstantExpression() = default;
ConstantExpression::ConstantExpression(ConstantExpression&&) noexcept = default;
ConstantExpression& ConstantExpression::operator=(ConstantExpression&&) noexcept = default;

org::minima::kissvm::values::Value* ConstantExpression::getValue(org::minima::kissvm::Contract& zContract) {
    // This action counts as one instruction
    zContract.incrementInstructions();

    // CRITICAL FIX: Return a COPY (owned pointer) instead of borrowed pointer
    // The constant is stored in this expression, but getValue() should return
    // an owned copy that the caller can delete
    if (!mValue) {
        throw org::minima::kissvm::exceptions::ExecutionException("ConstantExpression has null value");
    }
    
    return mValue->clone();
}

std::string ConstantExpression::toString() const {
    // The Java version returns mValue.toString().
    // The provided Value interface does not expose toString(), so we return the type string.
    if (mValue) {
        int vtype = mValue->getValueType();
        return org::minima::kissvm::values::Value::getValueTypeString(vtype);
    }
    return "null";
}

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org