#pragma once

#include <string>
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

class VariableExpression : public Expression {
public:
    explicit VariableExpression(const std::string& zName);

    // Special members
    ~VariableExpression() override = default;

    // Expression interface override
    // Throws org::minima::kissvm::exceptions::ExecutionException if variable does not exist
    org::minima::kissvm::values::Value* getValue(org::minima::kissvm::Contract& zContract) override;

    // String representation (not overriding any base virtual)
    std::string toString() const;

private:
    std::string mVariableName;
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org