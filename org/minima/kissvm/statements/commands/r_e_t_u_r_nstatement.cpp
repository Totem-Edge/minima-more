#include "org/minima/kissvm/statements/commands/r_e_t_u_r_nstatement.hpp"

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

#ifdef _WIN32
// No OS-specific behavior required for this class currently.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::expressions::Expression;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::BooleanValue;

RETURNstatement::RETURNstatement(std::unique_ptr<Expression> zReturnValue)
    : mReturnValue(std::move(zReturnValue)) {}

// Explicitly define special members (Pitfall 1)
RETURNstatement::~RETURNstatement() = default;
RETURNstatement::RETURNstatement(RETURNstatement&&) noexcept = default;
RETURNstatement& RETURNstatement::operator=(RETURNstatement&&) noexcept = default;

void RETURNstatement::execute(Contract& zContract) {
    // Calculate the value
    Value* val = mReturnValue->getValue(zContract);

    // MUST be a boolean
    if (!val || val->getValueType() != Value::VALUE_BOOLEAN) {
        throw ExecutionException(std::string("RETURN MUST use a BOOLEAN expression : ") + toString());
    }

    // Check it..
    BooleanValue* bval = static_cast<BooleanValue*>(val);

    // Tell the Contract
    zContract.setRETURNValue(bval->isTrue());
}

std::string RETURNstatement::toString() const {
    // Expression::toString is not available in the provided skeleton; provide a stable representation.
    return "RETURN <expr>";
}

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org