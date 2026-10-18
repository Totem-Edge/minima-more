#include "org/minima/kissvm/expressions/boolean_expression.hpp"

#include <utility>
#include <stdexcept>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

// Internal constant FALSE expression to mirror Java's ConstantExpression(FALSE)
namespace {
class ConstantFalseExpression final : public Expression {
public:
    ConstantFalseExpression() = default;
    ~ConstantFalseExpression() override = default;

    org::minima::kissvm::values::Value*
    getValue(org::minima::kissvm::Contract& zContract) override {
        // Count as one instruction (mirror expression behavior)
        zContract.incrementInstructions();
        return new org::minima::kissvm::values::BooleanValue(false);
    }

    // Not virtual in base; provided for potential diagnostics
    std::string toString() {
        return "FALSE";
    }
};
} // anonymous namespace

// Special members
BooleanExpression::~BooleanExpression() = default;
BooleanExpression::BooleanExpression(BooleanExpression&&) noexcept = default;
BooleanExpression& BooleanExpression::operator=(BooleanExpression&&) noexcept = default;

// Constructors
BooleanExpression::BooleanExpression(std::unique_ptr<Expression> zLeft, int zBooleanType)
    : mBooleanType(zBooleanType),
      mLeft(std::move(zLeft)),
      mRight(nullptr) {
    // For NOT, create a constant FALSE right-hand side to preserve evaluation semantics
    if (mBooleanType == BOOLEAN_NOT) {
        mRight = std::make_unique<ConstantFalseExpression>();
    }
}

BooleanExpression::BooleanExpression(std::unique_ptr<Expression> zLeft,
                                     std::unique_ptr<Expression> zRight,
                                     int zBooleanType)
    : mBooleanType(zBooleanType),
      mLeft(std::move(zLeft)),
      mRight(std::move(zRight)) {}

// Helper to get BooleanValue
org::minima::kissvm::values::BooleanValue
BooleanExpression::getBooleanValue(org::minima::kissvm::Contract& zContract) {
    using namespace org::minima::kissvm::values;
    using org::minima::kissvm::exceptions::ExecutionException;

    Value* val = getValue(zContract);
    if (!val) {
        throw ExecutionException("BooleanExpression::getBooleanValue returned null");
    }
    if (val->getValueType() != Value::VALUE_BOOLEAN) {
        // Clean up before throwing
        delete val;
        throw ExecutionException("Expected BOOLEAN in BooleanExpression::getBooleanValue");
    }
    auto* bval = static_cast<BooleanValue*>(val);
    bool res = bval->isTrue();
    delete val;
    return BooleanValue(res);
}

// Core evaluation
org::minima::kissvm::values::Value*
BooleanExpression::getValue(org::minima::kissvm::Contract& zContract) {
    using namespace org::minima::kissvm::values;
    using org::minima::kissvm::exceptions::ExecutionException;

    // This action counts as one instruction
    zContract.incrementInstructions();

    // Calculate the left and the right side
    Value* lval = mLeft ? mLeft->getValue(zContract) : nullptr;
    // Ensure mRight exists even for NOT (constructor may guarantee this)
    Value* rval = mRight ? mRight->getValue(zContract)
                         : static_cast<Value*>(new BooleanValue(false));

    // Only check for double value expressions
    if (mBooleanType != BOOLEAN_NOT) {
        Value::checkSameType(*lval, *rval);
    }

    Value* ret = nullptr;

    switch (mBooleanType) {
        // ONLY works for BOOLEAN value types
        case BOOLEAN_AND: {
            lval->verifyType(Value::VALUE_BOOLEAN);
            bool left  = static_cast<BooleanValue*>(lval)->isTrue();
            bool right = static_cast<BooleanValue*>(rval)->isTrue();
            ret = new BooleanValue(left && right);
            break;
        }
        case BOOLEAN_NAND: {
            lval->verifyType(Value::VALUE_BOOLEAN);
            bool left  = static_cast<BooleanValue*>(lval)->isTrue();
            bool right = static_cast<BooleanValue*>(rval)->isTrue();
            ret = new BooleanValue(!(left && right));
            break;
        }
        case BOOLEAN_OR: {
            lval->verifyType(Value::VALUE_BOOLEAN);
            bool left  = static_cast<BooleanValue*>(lval)->isTrue();
            bool right = static_cast<BooleanValue*>(rval)->isTrue();
            ret = new BooleanValue(left || right);
            break;
        }
        case BOOLEAN_NOR: {
            lval->verifyType(Value::VALUE_BOOLEAN);
            bool left  = static_cast<BooleanValue*>(lval)->isTrue();
            bool right = static_cast<BooleanValue*>(rval)->isTrue();
            ret = new BooleanValue(!(left || right));
            break;
        }
        case BOOLEAN_XOR: {
            lval->verifyType(Value::VALUE_BOOLEAN);
            bool left  = static_cast<BooleanValue*>(lval)->isTrue();
            bool right = static_cast<BooleanValue*>(rval)->isTrue();
            ret = new BooleanValue(left ^ right);
            break;
        }
        case BOOLEAN_NXOR: {
            lval->verifyType(Value::VALUE_BOOLEAN);
            bool left  = static_cast<BooleanValue*>(lval)->isTrue();
            bool right = static_cast<BooleanValue*>(rval)->isTrue();
            ret = new BooleanValue(!(left ^ right));
            break;
        }
        case BOOLEAN_NOT: {
            lval->verifyType(Value::VALUE_BOOLEAN);
            bool left = static_cast<BooleanValue*>(lval)->isTrue();
            ret = new BooleanValue(!left);
            break;
        }

        // Works for ANY value types
        case BOOLEAN_EQ: {
            int vtype = lval->getValueType();
            if (vtype == Value::VALUE_BOOLEAN) {
                bool left  = static_cast<BooleanValue*>(lval)->isTrue();
                bool right = static_cast<BooleanValue*>(rval)->isTrue();
                ret = new BooleanValue(left == right);
            } else if (vtype == Value::VALUE_HEX) {
                auto* lefthex  = static_cast<HexValue*>(lval);
                auto* righthex = static_cast<HexValue*>(rval);
                ret = new BooleanValue(lefthex->isEqual(*righthex));
            } else if (vtype == Value::VALUE_NUMBER) {
                auto* leftnum  = static_cast<NumberValue*>(lval);
                auto* rightnum = static_cast<NumberValue*>(rval);
                ret = new BooleanValue(leftnum->isEqual(*rightnum));
            } else if (vtype == Value::VALUE_SCRIPT) {
                auto* leftstr  = static_cast<StringValue*>(lval);
                auto* rightstr = static_cast<StringValue*>(rval);
                ret = new BooleanValue(leftstr->isEqual(*rightstr));
            }
            break;
        }
        case BOOLEAN_NEQ: {
            int vtype = lval->getValueType();
            if (vtype == Value::VALUE_BOOLEAN) {
                bool left  = static_cast<BooleanValue*>(lval)->isTrue();
                bool right = static_cast<BooleanValue*>(rval)->isTrue();
                ret = new BooleanValue(left != right);
            } else if (vtype == Value::VALUE_HEX) {
                auto* lefthex  = static_cast<HexValue*>(lval);
                auto* righthex = static_cast<HexValue*>(rval);
                ret = new BooleanValue(!lefthex->isEqual(*righthex));
            } else if (vtype == Value::VALUE_NUMBER) {
                auto* leftnum  = static_cast<NumberValue*>(lval);
                auto* rightnum = static_cast<NumberValue*>(rval);
                ret = new BooleanValue(!leftnum->isEqual(*rightnum));
            } else if (vtype == Value::VALUE_SCRIPT) {
                auto* leftstr  = static_cast<StringValue*>(lval);
                auto* rightstr = static_cast<StringValue*>(rval);
                ret = new BooleanValue(!leftstr->isEqual(*rightstr));
            }
            break;
        }

        // ONLY works for NUMBER value types
        case BOOLEAN_LT: {
            lval->verifyType(Value::VALUE_NUMBER);
            auto* leftnum  = static_cast<NumberValue*>(lval);
            auto* rightnum = static_cast<NumberValue*>(rval);
            ret = new BooleanValue(leftnum->isLess(*rightnum));
            break;
        }
        case BOOLEAN_LTE: {
            lval->verifyType(Value::VALUE_NUMBER);
            auto* leftnum  = static_cast<NumberValue*>(lval);
            auto* rightnum = static_cast<NumberValue*>(rval);
            ret = new BooleanValue(leftnum->isLessEqual(*rightnum));
            break;
        }
        case BOOLEAN_GT: {
            lval->verifyType(Value::VALUE_NUMBER);
            auto* leftnum  = static_cast<NumberValue*>(lval);
            auto* rightnum = static_cast<NumberValue*>(rval);
            ret = new BooleanValue(leftnum->isMore(*rightnum));
            break;
        }
        case BOOLEAN_GTE: {
            lval->verifyType(Value::VALUE_NUMBER);
            auto* leftnum  = static_cast<NumberValue*>(lval);
            auto* rightnum = static_cast<NumberValue*>(rval);
            ret = new BooleanValue(leftnum->isMoreEqual(*rightnum));
            break;
        }

        default:
            // Clean up before throwing
            delete lval;
            delete rval;
            throw ExecutionException("UNKNOWN boolean operator : " + std::to_string(mBooleanType));
    }

    // And trace it..
    if (ret) {
        zContract.traceLog(toString() + " returns:" + ret->toString());
    }

    // Clean up temporaries before returning result
    delete lval;
    delete rval;

    return ret;
}

std::string BooleanExpression::toString() {
    std::string op = "ERROR";

    switch (mBooleanType) {
        case BOOLEAN_AND:  op = "AND";  break;
        case BOOLEAN_NAND: op = "NAND"; break;
        case BOOLEAN_OR:   op = "OR";   break;
        case BOOLEAN_NOR:  op = "NOR";  break;
        case BOOLEAN_XOR:  op = "XOR";  break;
        case BOOLEAN_NXOR: op = "NXOR"; break;

        case BOOLEAN_EQ:   op = "EQ";   break;
        case BOOLEAN_NEQ:  op = "NEQ";  break;

        case BOOLEAN_LT:   op = "LT";   break;
        case BOOLEAN_LTE:  op = "LTE";  break;

        case BOOLEAN_GT:   op = "GT";   break;
        case BOOLEAN_GTE:  op = "GTE";  break;

        case BOOLEAN_NOT:
            // Cannot call sub-expression toString() (not virtual in base)
            return "NOT ( EXPR )";
    }

    // Cannot call sub-expression toString() (not virtual in base)
    return "( EXPR " + op + " EXPR )";
}

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org