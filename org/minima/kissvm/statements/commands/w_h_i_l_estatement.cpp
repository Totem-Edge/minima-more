#include "org/minima/kissvm/statements/commands/w_h_i_l_estatement.hpp"

#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/statements/statement_block.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

#ifdef _WIN32
// No OS-specific behavior required for this translation.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

WHILEstatement::WHILEstatement(
    std::unique_ptr<org::minima::kissvm::expressions::Expression> zWhileCheck,
    std::unique_ptr<org::minima::kissvm::statements::StatementBlock> zCodeBlock)
    : mWhileCheck(std::move(zWhileCheck)),
      mWhileBlock(std::move(zCodeBlock)) {}

WHILEstatement::~WHILEstatement() = default;
WHILEstatement::WHILEstatement(WHILEstatement&&) noexcept = default;
WHILEstatement& WHILEstatement::operator=(WHILEstatement&&) noexcept = default;

void WHILEstatement::execute(org::minima::kissvm::Contract& zContract) {
    using org::minima::kissvm::exceptions::ExecutionException;
    using org::minima::kissvm::values::Value;
    using org::minima::kissvm::values::BooleanValue;

    // Calculate the value..
    Value* val = mWhileCheck->getValue(zContract);

    // MUST be a boolean
    if (!val || val->getValueType() != Value::VALUE_BOOLEAN) {
        throw ExecutionException("RETURN MUST use a BOOLEAN expression : " + toString());
    }

    // Check it..
    BooleanValue* bval = static_cast<BooleanValue*>(val);

    // Loop while true
    while (bval->isTrue()) {
        // Run the code..
        mWhileBlock->run(zContract);

        // Check for EXIT
        if (zContract.isSuccessSet()) {
            return;
        }

        // Recalculate the value..
        val = mWhileCheck->getValue(zContract);

        // MUST be a boolean
        if (!val || val->getValueType() != Value::VALUE_BOOLEAN) {
            throw ExecutionException("RETURN MUST use a BOOLEAN expression : " + toString());
        }

        // Check it..
        bval = static_cast<BooleanValue*>(val);
    }
}

std::string WHILEstatement::toString() const {
    // Keep it simple; Expression doesn't guarantee a toString interface in provided headers
    return "WHILE";
}

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org