#pragma once

#include <string>
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

class GlobalExpression : public Expression {
public:
    explicit GlobalExpression(const std::string& zType);

    // Special members
    ~GlobalExpression() override = default;

    // Expression interface override
    org::minima::kissvm::values::Value* getValue(org::minima::kissvm::Contract& zContract) override;

    // String representation (not overriding any base virtual)
    std::string toString() const;

private:
    std::string mGlobalType;
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org