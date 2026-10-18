#include "org/minima/kissvm/statements/commands/i_fstatement.hpp"

#include <sstream>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/statements/statement_block.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

#ifdef _WIN32
// No OS-specific behavior required here; placeholder for future differences
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

IFstatement::IFstatement() = default;

// Define special members (Pitfall 1)
IFstatement::~IFstatement() = default;
IFstatement::IFstatement(IFstatement&&) noexcept = default;
IFstatement& IFstatement::operator=(IFstatement&&) noexcept = default;

void IFstatement::addCondition(
    std::unique_ptr<org::minima::kissvm::expressions::Expression> zCondition,
    std::unique_ptr<org::minima::kissvm::statements::StatementBlock> zCodeBlock) 
{
    mConditions.emplace_back(std::move(zCondition));
    mActions.emplace_back(std::move(zCodeBlock));
}

void IFstatement::execute(org::minima::kissvm::Contract& zContract) {
    using org::minima::kissvm::exceptions::ExecutionException;
    using org::minima::kissvm::values::Value;
    using org::minima::kissvm::values::BooleanValue;

    const std::size_t size = mConditions.size();

    for (std::size_t loop = 0; loop < size; ++loop) {
        // Defensive: ensure parallel vectors remain aligned
        if (loop >= mActions.size() || !mConditions[loop] || !mActions[loop]) {
            throw ExecutionException("IFstatement internal error: mismatched condition/action lists");
        }

        // Evaluate the condition
        Value* val = mConditions[loop]->getValue(zContract);

        // MUST be a boolean
        if (!val || val->getValueType() != Value::VALUE_BOOLEAN) {
            // We do not have Expression::toString(), so fallback to a generic description
            throw ExecutionException("IF conditional MUST use a BOOLEAN expression");
        }

        // Cast and check
        auto* bval = dynamic_cast<BooleanValue*>(val);
        if (!bval) {
            throw ExecutionException("IF conditional MUST use a BOOLEAN expression");
        }

        if (bval->isTrue()) {
            // Run the associated code block
            mActions[loop]->run(zContract);
            // Done with IF chain
            break;
        }
    }
}

std::string IFstatement::toString() const {
    // We don't have Expression::toString() in the provided headers.
    // Provide a structurally similar string with placeholders.
    std::ostringstream oss;

    const std::size_t size = mConditions.size();
    for (std::size_t loop = 0; loop < size; ++loop) {
        const char* prefix = (loop == 0) ? "IF " : ", ELSEIF ";
        oss << prefix << "[EXPR]";
    }

    return oss.str();
}

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org