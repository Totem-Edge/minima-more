#include "org/minima/kissvm/functions/cast/s_t_r_i_n_g.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

STRING::STRING()
    : org::minima::kissvm::functions::MinimaFunction("STRING") { }

std::unique_ptr<org::minima::kissvm::values::Value>
STRING::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exactly 1 parameter
    checkExactParamNumber(requiredParams());

    // Evaluate the first parameter to a Value
    auto val = getParameter(0).getValue(zContract);

    // Convert to string using Value's toString and wrap in StringValue
    std::string out = val->toString();
    return std::make_unique<org::minima::kissvm::values::StringValue>(out);
}

int STRING::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> STRING::getNewFunction() {
    return std::make_unique<STRING>();
}

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org