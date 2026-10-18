#include "org/minima/kissvm/expressions/function_expression.hpp"

#include <utility>
#include <vector>
#include <string>

// Full headers for used types
#include "org/minima/kissvm/functions/minima_function.hpp"
#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

FunctionExpression::FunctionExpression(std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> zFunction)
    : mFunction(std::move(zFunction)) {}

// Define special members (Rule 7)
FunctionExpression::~FunctionExpression() = default;
FunctionExpression::FunctionExpression(FunctionExpression&&) noexcept = default;
FunctionExpression& FunctionExpression::operator=(FunctionExpression&&) noexcept = default;

org::minima::kissvm::values::Value*
FunctionExpression::getValue(org::minima::kissvm::Contract& zContract) {
    // This action counts as one instruction
    zContract.incrementInstructions();

    // Increment Stack Depth
    zContract.incrementStackDepth();

    // Get the Value
    std::unique_ptr<org::minima::kissvm::values::Value> val_up = mFunction->runFunction(zContract);
    org::minima::kissvm::values::Value* val_raw = val_up.get();

    // Decrement Stack Depth
    zContract.decrementStackDepth();

    // And trace it.. use type string since Value base has no toString()
    std::string valdesc = (val_raw)
        ? org::minima::kissvm::values::Value::getValueTypeString(val_raw->getValueType())
        : std::string("null");
    zContract.traceLog(toString() + " returns:" + valdesc);

    // Transfer ownership to caller to match Expression::getValue signature
    return val_up.release();
}

std::string FunctionExpression::toString() const {
    // We cannot stringify parameters via Expression::toString (not part of the interface),
    // so provide function name and parameter count.
    std::string out = "function:" + mFunction->getName() + ", params:" + std::to_string(mFunction->getParameterNum());
    return out;
}

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org