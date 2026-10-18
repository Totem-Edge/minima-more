#include "org/minima/kissvm/expressions/global_expression.hpp"

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

GlobalExpression::GlobalExpression(const std::string& zType)
    : mGlobalType(zType) {
}

org::minima::kissvm::values::Value*
GlobalExpression::getValue(org::minima::kissvm::Contract& zContract) {
    // This action counts as one instruction
    zContract.incrementInstructions();

    // Obtain the const reference from the contract and return a non-owning pointer
    const org::minima::kissvm::values::Value& val = zContract.getGlobal(mGlobalType);
    
    // CRITICAL FIX: Return a COPY (owned pointer) instead of borrowed pointer
    // This makes ownership consistent with FunctionExpression
    return val.clone();
}

std::string GlobalExpression::toString() const {
    return std::string("global:") + mGlobalType;
}

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org