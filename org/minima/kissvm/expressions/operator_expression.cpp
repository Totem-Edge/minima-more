#include "org/minima/kissvm/expressions/operator_expression.hpp"

#include <vector>
#include <cstdint>
#include <sstream>
#include <algorithm>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"

#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::StringValue;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

OperatorExpression::OperatorExpression(std::unique_ptr<Expression> zLeft, int zOperator)
    : mOperatorType(zOperator)
    , mLeft(std::move(zLeft))
    , mRight(nullptr) {
    // Unary operators in Java used a ConstantExpression(FALSE) as RHS, which would
    // increment instructions when evaluated. We emulate that during getValue().
}

OperatorExpression::OperatorExpression(std::unique_ptr<Expression> zLeft,
                                       std::unique_ptr<Expression> zRight,
                                       int zOperator)
    : mOperatorType(zOperator)
    , mLeft(std::move(zLeft))
    , mRight(std::move(zRight)) {
}

OperatorExpression::~OperatorExpression() = default;
OperatorExpression::OperatorExpression(OperatorExpression&&) noexcept = default;
OperatorExpression& OperatorExpression::operator=(OperatorExpression&&) noexcept = default;

Value* OperatorExpression::getValue(org::minima::kissvm::Contract& zContract) {
    Value* ret = nullptr;

    // This action counts as one instruction
    zContract.incrementInstructions();

    // Always evaluate the left side
    Value* lval = mLeft ? mLeft->getValue(zContract) : nullptr;

    switch (mOperatorType) {
        // ADD: works with Number OR SCRIPT
        case OPERATOR_ADD: {
            if (!mRight) {
                throw ExecutionException("ADD requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval);

            if (lval->getValueType() == Value::VALUE_NUMBER) {
                auto* lnv = dynamic_cast<NumberValue*>(lval);
                auto* rnv = dynamic_cast<NumberValue*>(rval);
                if (!lnv || !rnv) {
                    throw ExecutionException("Invalid type in ADD. MUST be Number or String " + lval->toString());
                }
                MiniNumber sum = lnv->getNumber().add(rnv->getNumber());
                ret = new NumberValue(sum);
            } else if (lval->getValueType() == Value::VALUE_SCRIPT) {
                auto* lsv = dynamic_cast<StringValue*>(lval);
                auto* rsv = dynamic_cast<StringValue*>(rval);
                if (!lsv || !rsv) {
                    throw ExecutionException("Invalid type in ADD. MUST be Number or String " + lval->toString());
                }
                StringValue sres = lsv->add(*rsv); // returns by value
                ret = new StringValue(std::move(sres));
            } else {
                throw ExecutionException("Invalid type in ADD. MUST be Number or String " + lval->toString());
            }
        } break;

        // Numeric-only operators
        case OPERATOR_SUB: {
            if (!mRight) {
                throw ExecutionException("SUB requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval, Value::VALUE_NUMBER);
            auto* lnv = dynamic_cast<NumberValue*>(lval);
            auto* rnv = dynamic_cast<NumberValue*>(rval);
            if (!lnv || !rnv) {
                throw ExecutionException("Invalid type in SUB.");
            }
            MiniNumber diff = lnv->getNumber().sub(rnv->getNumber());
            ret = new NumberValue(diff);
        } break;

        case OPERATOR_MUL: {
            if (!mRight) {
                throw ExecutionException("MUL requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval, Value::VALUE_NUMBER);
            auto* lnv = dynamic_cast<NumberValue*>(lval);
            auto* rnv = dynamic_cast<NumberValue*>(rval);
            if (!lnv || !rnv) {
                throw ExecutionException("Invalid type in MUL.");
            }
            MiniNumber prod = lnv->getNumber().mult(rnv->getNumber());
            ret = new NumberValue(prod);
        } break;

        case OPERATOR_DIV: {
            if (!mRight) {
                throw ExecutionException("DIV requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval, Value::VALUE_NUMBER);
            auto* lnv = dynamic_cast<NumberValue*>(lval);
            auto* rnv = dynamic_cast<NumberValue*>(rval);
            if (!lnv || !rnv) {
                throw ExecutionException("Invalid type in DIV.");
            }
            if (rnv->getNumber().isEqual(MiniNumber::ZERO())) {
                throw ExecutionException(std::string("Divide By ZERO! ") + toString());
            }
            MiniNumber quot = lnv->getNumber().div(rnv->getNumber());
            ret = new NumberValue(quot);
        } break;

        case OPERATOR_NEG: {
            // Java evaluated a dummy RHS that increments instructions. Emulate that.
            if (!mRight) {
                zContract.incrementInstructions();
            }
            lval->verifyType(Value::VALUE_NUMBER);
            auto* lnv = dynamic_cast<NumberValue*>(lval);
            if (!lnv) {
                throw ExecutionException("Invalid type in NEG.");
            }
            MiniNumber neg = lnv->getNumber().mult(MiniNumber::MINUSONE());
            ret = new NumberValue(neg);
        } break;

        case OPERATOR_MODULO: {
            if (!mRight) {
                throw ExecutionException("MODULO requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval, Value::VALUE_NUMBER);
            auto* lnv = dynamic_cast<NumberValue*>(lval);
            auto* rnv = dynamic_cast<NumberValue*>(rval);
            if (!lnv || !rnv) {
                throw ExecutionException("Invalid type in MODULO.");
            }
            MiniNumber mod = lnv->getNumber().modulo(rnv->getNumber());
            ret = new NumberValue(mod);
        } break;

        // HEX + NUMBER (shift)
        case OPERATOR_SHIFTL: {
            if (!mRight) {
                throw ExecutionException("SHIFTL requires right expression");
            }
            Value* rval = mRight->getValue(zContract);

            lval->verifyType(Value::VALUE_HEX);
            rval->verifyType(Value::VALUE_NUMBER);

            auto* lhv = dynamic_cast<HexValue*>(lval);
            auto* rnv = dynamic_cast<NumberValue*>(rval);
            if (!lhv || !rnv) {
                throw ExecutionException("Invalid types in SHIFTL.");
            }

            //
            // FIX: Use the static accessor from Contract
            //
            if (rnv->getNumber().abs().isMore(org::minima::kissvm::Contract::getMAX_BITSHIFT())) {
                throw ExecutionException(std::string("Can only SHIFTLEFT ")
                    //
                    // FIX: Use the static accessor from Contract
                    //
                    + org::minima::kissvm::Contract::getMAX_BITSHIFT().toString()
                    + " bits MAX " + rnv->getNumber().toString());
            }

            MiniData shifted = lhv->getMiniData().shiftl(rnv->getNumber().getAsInt());
            ret = new HexValue(shifted);
        } break;

        case OPERATOR_SHIFTR: {
            if (!mRight) {
                throw ExecutionException("SHIFTR requires right expression");
            }
            Value* rval = mRight->getValue(zContract);

            lval->verifyType(Value::VALUE_HEX);
            rval->verifyType(Value::VALUE_NUMBER);

            auto* lhv = dynamic_cast<HexValue*>(lval);
            auto* rnv = dynamic_cast<NumberValue*>(rval);
            if (!lhv || !rnv) {
                throw ExecutionException("Invalid types in SHIFTR.");
            }

            //
            // FIX: Use the static accessor from Contract
            //
            if (rnv->getNumber().abs().isMore(org::minima::kissvm::Contract::getMAX_BITSHIFT())) {
                throw ExecutionException(std::string("Can only SHIFTRIGHT ")
                    //
                    // FIX: Use the static accessor from Contract
                    //
                    + org::minima::kissvm::Contract::getMAX_BITSHIFT().toString()
                    + " bits MAX " + rnv->getNumber().toString());
            }

            MiniData shifted = lhv->getMiniData().shiftr(rnv->getNumber().getAsInt());
            ret = new HexValue(shifted);
        } break;

        // HEX-only bitwise
        case OPERATOR_AND: {
// ... (rest of file is unchanged) ...
            if (!mRight) {
                throw ExecutionException("AND requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval, Value::VALUE_HEX);
            auto* lhv = dynamic_cast<HexValue*>(lval);
            auto* rhv = dynamic_cast<HexValue*>(rval);
            if (!lhv || !rhv) {
                throw ExecutionException("Invalid type in AND.");
            }
            MiniData result = andFastHEX(lhv->getMiniData(), rhv->getMiniData());
            ret = new HexValue(result);
        } break;

        case OPERATOR_OR: {
            if (!mRight) {
                throw ExecutionException("OR requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval, Value::VALUE_HEX);
            auto* lhv = dynamic_cast<HexValue*>(lval);
            auto* rhv = dynamic_cast<HexValue*>(rval);
            if (!lhv || !rhv) {
                throw ExecutionException("Invalid type in OR.");
            }
            MiniData result = orFastHEX(lhv->getMiniData(), rhv->getMiniData(), 0);
            ret = new HexValue(result);
        } break;

        case OPERATOR_XOR: {
            if (!mRight) {
                throw ExecutionException("XOR requires right expression");
            }
            Value* rval = mRight->getValue(zContract);
            Value::checkSameType(*lval, *rval, Value::VALUE_HEX);
            auto* lhv = dynamic_cast<HexValue*>(lval);
            auto* rhv = dynamic_cast<HexValue*>(rval);
            if (!lhv || !rhv) {
                throw ExecutionException("Invalid type in XOR.");
            }
            MiniData result = orFastHEX(lhv->getMiniData(), rhv->getMiniData(), 1);
            ret = new HexValue(result);
        } break;

        case OPERATOR_NOT: {
            // Java evaluated a dummy RHS that increments instructions. Emulate that.
            if (!mRight) {
                zContract.incrementInstructions();
            }
            lval->verifyType(Value::VALUE_HEX);
            auto* lhv = dynamic_cast<HexValue*>(lval);
            if (!lhv) {
                throw ExecutionException("Invalid type in NOT.");
            }
            MiniData result = notFastHEX(lhv->getMiniData());
            ret = new HexValue(result);
        } break;

        default:
            throw ExecutionException("UNKNOWN operator");
    }

    // Trace it..
    if (ret) {
        zContract.traceLog(toString() + " returns:" + ret->toString());
    }

    return ret;
}

std::string OperatorExpression::toString() const {
    std::string op = "ERROR";

    switch (mOperatorType) {
        case OPERATOR_ADD:     op = "+";  break;
        case OPERATOR_SUB:     op = "-";  break;
        case OPERATOR_MUL:     op = "*";  break;
        case OPERATOR_DIV:     op = "/";  break;
        case OPERATOR_MODULO:  op = "%";  break;
        case OPERATOR_SHIFTL:  op = "<<"; break;
        case OPERATOR_SHIFTR:  op = ">>"; break;
        case OPERATOR_NEG:
            return " - ( ... )";
        case OPERATOR_AND:     op = "&";  break;
        case OPERATOR_OR:      op = "|";  break;
        case OPERATOR_XOR:     op = "^";  break;
        case OPERATOR_NOT:
            return " ~ ( ... )";
        default: break;
    }

    // We cannot call child toString() (Expression base doesn't expose it here),
    // so elide child details to keep compilation consistent.
    return "( ... " + op + " ... )";
}

// Private static helpers

MiniData OperatorExpression::andFastHEX(const MiniData& zHex1, const MiniData& zHex2) {
    const std::vector<uint8_t>& bytesh1 = zHex1.getBytes();
    const std::vector<uint8_t>& bytesh2 = zHex2.getBytes();

    int len1 = static_cast<int>(bytesh1.size());
    int len2 = static_cast<int>(bytesh2.size());

    bool hex1shorter = true;
    int minlen = len1;
    if (len2 < minlen) {
        minlen = len2;
        hex1shorter = false;
    }

    std::vector<uint8_t> pbytes1(minlen, 0);
    std::vector<uint8_t> pbytes2(minlen, 0);

    if (hex1shorter) {
        // Copy data
        if (len1 >= minlen) {
            std::copy(bytesh1.begin(), bytesh1.begin() + minlen, pbytes1.begin());
        } else {
            std::copy(bytesh1.begin(), bytesh1.end(), pbytes1.begin());
        }
        if (len2 >= minlen) {
            std::copy(bytesh2.begin() + (len2 - minlen), bytesh2.begin() + len2, pbytes2.begin());
        } else {
            std::copy(bytesh2.begin(), bytesh2.end(), pbytes2.begin() + (minlen - len2));
        }
    } else {
        // Copy data
        if (len1 >= minlen) {
            std::copy(bytesh1.begin() + (len1 - minlen), bytesh1.begin() + len1, pbytes1.begin());
        } else {
            std::copy(bytesh1.begin(), bytesh1.end(), pbytes1.begin() + (minlen - len1));
        }
        if (len2 >= minlen) {
            std::copy(bytesh2.begin(), bytesh2.begin() + minlen, pbytes2.begin());
        } else {
            std::copy(bytesh2.begin(), bytesh2.end(), pbytes2.begin());
        }
    }

    std::vector<uint8_t> result(minlen, 0);
    bool nonzerofound = false;
    int counter = 0;

    for (int i = 0; i < minlen; ++i) {
        uint8_t bres = static_cast<uint8_t>(pbytes1[i] & pbytes2[i]);

        if (nonzerofound) {
            result[counter++] = bres;
        } else {
            if (bres != 0) {
                nonzerofound = true;
                result[counter++] = bres;
            }
        }
    }

    if (counter == 0) {
        return MiniData(std::string("0x00"));
    }

    std::vector<uint8_t> finalresult(result.begin(), result.begin() + counter);
    return MiniData(finalresult);
}

MiniData OperatorExpression::orFastHEX(const MiniData& zHex1, const MiniData& zHex2, int zType) {
    const std::vector<uint8_t>& bytesh1 = zHex1.getBytes();
    const std::vector<uint8_t>& bytesh2 = zHex2.getBytes();

    int len1 = static_cast<int>(bytesh1.size());
    int len2 = static_cast<int>(bytesh2.size());

    bool hex1longer = true;
    int maxlen = len1;
    if (len2 > maxlen) {
        maxlen = len2;
        hex1longer = false;
    }

    std::vector<uint8_t> pbytes1(maxlen, 0);
    std::vector<uint8_t> pbytes2(maxlen, 0);

    if (hex1longer) {
        // Copy data
        if (len1 >= maxlen) {
            std::copy(bytesh1.begin(), bytesh1.begin() + maxlen, pbytes1.begin());
        } else {
            std::copy(bytesh1.begin(), bytesh1.end(), pbytes1.begin());
        }

        // pbytes2 starts at maxlen - len2
        if (len2 <= maxlen) {
            std::copy(bytesh2.begin(), bytesh2.end(), pbytes2.begin() + (maxlen - len2));
        } else {
            std::copy(bytesh2.begin() + (len2 - maxlen), bytesh2.begin() + len2, pbytes2.begin());
        }
    } else {
        // hex2 longer or equal
        if (len1 <= maxlen) {
            std::copy(bytesh1.begin(), bytesh1.end(), pbytes1.begin() + (maxlen - len1));
        } else {
            std::copy(bytesh1.begin(), bytesh1.begin() + maxlen, pbytes1.begin());
        }

        if (len2 >= maxlen) {
            std::copy(bytesh2.begin(), bytesh2.begin() + maxlen, pbytes2.begin());
        } else {
            std::copy(bytesh2.begin(), bytesh2.end(), pbytes2.begin());
        }
    }

    std::vector<uint8_t> result(maxlen, 0);
    bool nonzerofound = false;
    int counter = 0;

    if (zType == 0) {
        // OR
        for (int i = 0; i < maxlen; ++i) {
            uint8_t bres = static_cast<uint8_t>(pbytes1[i] | pbytes2[i]);

            if (nonzerofound) {
                result[counter++] = bres;
            } else {
                if (bres != 0) {
                    nonzerofound = true;
                    result[counter++] = bres;
                }
            }
        }
    } else {
        // XOR
        for (int i = 0; i < maxlen; ++i) {
            uint8_t bres = static_cast<uint8_t>(pbytes1[i] ^ pbytes2[i]);

            if (nonzerofound) {
                result[counter++] = bres;
            } else {
                if (bres != 0) {
                    nonzerofound = true;
                    result[counter++] = bres;
                }
            }
        }
    }

    if (counter == 0) {
        return MiniData(std::string("0x00"));
    }

    std::vector<uint8_t> finalresult(result.begin(), result.begin() + counter);
    return MiniData(finalresult);
}

MiniData OperatorExpression::notFastHEX(const MiniData& zHex1) {
    const std::vector<uint8_t>& bytesh1 = zHex1.getBytes();

    int len = static_cast<int>(bytesh1.size());
    std::vector<uint8_t> result(len, 0);

    bool nonzerofound = false;
    int counter = 0;

    for (int i = 0; i < len; ++i) {
        uint8_t bres = static_cast<uint8_t>(~bytesh1[i]);

        if (nonzerofound) {
            result[counter++] = bres;
        } else {
            if (bres != 0) {
                nonzerofound = true;
                result[counter++] = bres;
            }
        }
    }

    if (counter == 0) {
        return MiniData(std::string("0x00"));
    }

    std::vector<uint8_t> finalresult(result.begin(), result.begin() + counter);
    return MiniData(finalresult);
}

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org
