#include "org/minima/kissvm/statements/commands/a_s_s_e_r_tstatement.hpp"

#include <utility>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

#ifdef _WIN32
// No Windows-specific logic required here; placeholder to show conditional compilation is considered.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

ASSERTstatement::ASSERTstatement(std::unique_ptr<org::minima::kissvm::expressions::Expression> zAssertValue)
    : mAssertValue(std::move(zAssertValue)) {}

// Define special members (Pitfall 1: unique_ptr to forward-declared type)
ASSERTstatement::~ASSERTstatement() = default;
ASSERTstatement::ASSERTstatement(ASSERTstatement&&) noexcept = default;
ASSERTstatement& ASSERTstatement::operator=(ASSERTstatement&&) noexcept = default;

void ASSERTstatement::execute(org::minima::kissvm::Contract& zContract) {
    // Get the expression value
    org::minima::kissvm::values::Value* val = mAssertValue->getValue(zContract);

    // MUST be a boolean
    if (!val || val->getValueType() != org::minima::kissvm::values::Value::VALUE_BOOLEAN) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            std::string("ASSERT MUST use a BOOLEAN expression : ") + toString()
        );
    }

    // Does it pass
    auto* bval = static_cast<org::minima::kissvm::values::BooleanValue*>(val);
    bool success = bval->isTrue();

    // Tell the Contract to FAIL if FALSE
    if (!success) {
        zContract.setRETURNValue(false);
    }
}

std::string ASSERTstatement::toString() const {
    // Expression base class in provided headers does not expose toString; provide a stable fallback.
    return "ASSERT <expr>";
}

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org