#include "org/minima/kissvm/expressions/variable_expression.hpp"

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

VariableExpression::VariableExpression(const std::string& zName)
    : mVariableName(zName) {
}

org::minima::kissvm::values::Value* 
VariableExpression::getValue(org::minima::kissvm::Contract& zContract) {
    zContract.incrementInstructions();
    
    const org::minima::kissvm::values::Value* val = zContract.getVariable(mVariableName);
    
    if (val == nullptr) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            std::string("Variable does not exist : ") + mVariableName);
    }
    
    // Return a COPY - caller owns it and must delete
    return val->clone();
}

std::string VariableExpression::toString() const {
    return std::string("variable:") + mVariableName;
}

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org