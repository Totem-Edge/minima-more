#pragma once

#include <memory>
#include <string>

// Forward declarations
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

// CRITICAL: Must include the base class!
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

// CRITICAL FIX: Must inherit from Expression!
class ConstantExpression : public Expression {
public:
    explicit ConstantExpression(std::unique_ptr<org::minima::kissvm::values::Value> zValue);

    // Destructor and move operations explicitly declared due to unique_ptr to forward-declared type
    ~ConstantExpression() override;  // Mark as override
    ConstantExpression(ConstantExpression&&) noexcept;
    ConstantExpression& operator=(ConstantExpression&&) noexcept;

    // No copy
    ConstantExpression(const ConstantExpression&) = delete;
    ConstantExpression& operator=(const ConstantExpression&) = delete;

    // CRITICAL FIX: Mark as override to ensure it matches base class signature
    org::minima::kissvm::values::Value* getValue(org::minima::kissvm::Contract& zContract) override;

    // Best-effort string representation given available API in Value
    std::string toString() const;

private:
    std::unique_ptr<org::minima::kissvm::values::Value> mValue;
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org