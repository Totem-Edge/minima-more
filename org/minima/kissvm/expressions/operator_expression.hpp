#pragma once

#include <memory>
#include <string>

// Base class include (inheritance requires full type)
#include "org/minima/kissvm/expressions/expression.hpp"

// Forward declarations for project types used in members/signatures
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; class NumberValue; class StringValue; class HexValue; } } } }
namespace org { namespace minima { namespace kissvm { namespace exceptions { class ExecutionException; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

class OperatorExpression : public Expression {
public:
    // Operator types (match Java constants)
    static constexpr int OPERATOR_ADD    = 0;
    static constexpr int OPERATOR_SUB    = 1;
    static constexpr int OPERATOR_MUL    = 2;
    static constexpr int OPERATOR_DIV    = 3;

    static constexpr int OPERATOR_NEG    = 4;

    static constexpr int OPERATOR_SHIFTL = 5;
    static constexpr int OPERATOR_SHIFTR = 6;

    static constexpr int OPERATOR_MODULO = 7;

    static constexpr int OPERATOR_AND    = 8;
    static constexpr int OPERATOR_OR     = 9;
    static constexpr int OPERATOR_XOR    = 10;
    static constexpr int OPERATOR_NOT    = 11;

    // Used for NOT/NEG unary operators
    OperatorExpression(std::unique_ptr<Expression> zLeft, int zOperator);

    // General binary constructor
    OperatorExpression(std::unique_ptr<Expression> zLeft,
                       std::unique_ptr<Expression> zRight,
                       int zOperator);

    // Special members due to unique_ptr members
    ~OperatorExpression() override;
    OperatorExpression(OperatorExpression&&) noexcept;
    OperatorExpression& operator=(OperatorExpression&&) noexcept;

    // Delete copy operations
    OperatorExpression(const OperatorExpression&) = delete;
    OperatorExpression& operator=(const OperatorExpression&) = delete;

    // Expression interface override
    org::minima::kissvm::values::Value* getValue(org::minima::kissvm::Contract& zContract) override;

    // String representation (no 'override' since base may not declare it)
    std::string toString() const;

private:
    int mOperatorType;
    std::unique_ptr<Expression> mLeft;
    std::unique_ptr<Expression> mRight;

    // Hex helpers (ported from Java fast implementations)
    static org::minima::objects::base::MiniData andFastHEX(
        const org::minima::objects::base::MiniData& zHex1,
        const org::minima::objects::base::MiniData& zHex2);

    // zType: 0 = OR, 1 = XOR
    static org::minima::objects::base::MiniData orFastHEX(
        const org::minima::objects::base::MiniData& zHex1,
        const org::minima::objects::base::MiniData& zHex2,
        int zType);

    static org::minima::objects::base::MiniData notFastHEX(
        const org::minima::objects::base::MiniData& zHex1);
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org