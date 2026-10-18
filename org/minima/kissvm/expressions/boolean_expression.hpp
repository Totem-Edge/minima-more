#pragma once

#include <memory>
#include <string>

// Inheritance requires full base class include
#include "org/minima/kissvm/expressions/expression.hpp"

// Forward declarations to avoid heavy includes in header
namespace org { namespace minima { namespace kissvm { class Contract; } } }

namespace org { namespace minima { namespace kissvm { namespace values {
    class Value;
    class BooleanValue;
} } } }

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

class BooleanExpression : public Expression {
public:
    // Boolean operation types
    static constexpr int BOOLEAN_AND  = 0;
    static constexpr int BOOLEAN_NAND = 1;
    static constexpr int BOOLEAN_OR   = 2;
    static constexpr int BOOLEAN_NOR  = 3;
    static constexpr int BOOLEAN_XOR  = 4;
    static constexpr int BOOLEAN_NXOR = 5;

    static constexpr int BOOLEAN_EQ   = 6;
    static constexpr int BOOLEAN_NEQ  = 7;
    static constexpr int BOOLEAN_LT   = 8;
    static constexpr int BOOLEAN_LTE  = 9;
    static constexpr int BOOLEAN_GT   = 10;
    static constexpr int BOOLEAN_GTE  = 11;

    static constexpr int BOOLEAN_NOT  = 12;

    // Used for NOT (creates an internal FALSE right-hand side)
    explicit BooleanExpression(std::unique_ptr<Expression> zLeft, int zBooleanType);

    // General constructor
    BooleanExpression(std::unique_ptr<Expression> zLeft,
                      std::unique_ptr<Expression> zRight,
                      int zBooleanType);

    // Special members due to unique_ptr members
    ~BooleanExpression() override;
    BooleanExpression(BooleanExpression&&) noexcept;
    BooleanExpression& operator=(BooleanExpression&&) noexcept;

    BooleanExpression(const BooleanExpression&) = delete;
    BooleanExpression& operator=(const BooleanExpression&) = delete;

    // Helper to get BooleanValue result (returns by value, like Java)
    org::minima::kissvm::values::BooleanValue getBooleanValue(org::minima::kissvm::Contract& zContract);

    // Expression interface override
    org::minima::kissvm::values::Value* getValue(org::minima::kissvm::Contract& zContract) override;

    // String representation (no override - base does not declare this)
    std::string toString();

private:
    int mBooleanType;
    std::unique_ptr<Expression> mLeft;
    std::unique_ptr<Expression> mRight;
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org