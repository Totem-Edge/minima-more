#include "org/minima/kissvm/functions/cast/h_e_x.hpp"

#include <vector>
#include <cstdint>
#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::functions::MinimaFunction;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::StringValue;
using org::minima::objects::base::MiniNumber;

HEX::HEX() : MinimaFunction("HEX") {}

std::unique_ptr<Value> HEX::runFunction(Contract& zContract) {
    checkExactParamNumber(requiredParams());

    // Get the Value (Expression API returns a raw pointer)
    Value* val = getParameter(0).getValue(zContract);

    // Type switch
    int type = val->getValueType();

    if (type == Value::VALUE_BOOLEAN) {
        auto* cval = static_cast<BooleanValue*>(val);
        if (cval->isTrue()) {
            return std::make_unique<HexValue>(std::string("0x01"));
        } else {
            return std::make_unique<HexValue>(std::string("0x00"));
        }
    } else if (type == Value::VALUE_HEX) {
        auto* cval = static_cast<HexValue*>(val);
        return std::make_unique<HexValue>(cval->getMiniData());
    } else if (type == Value::VALUE_NUMBER) {
        auto* cval = static_cast<NumberValue*>(val);

        // Check no decimal places and non-negative
        MiniNumber num = cval->getNumber();
        if (!num.floor().isEqual(num) || num.isLess(MiniNumber::ZERO())) {
            throw ExecutionException(
                std::string("Can ONLY convert positive whole NUMBERs to HEX : ") + num.toString());
        }

        // Construct HexValue from MiniNumber (equivalent to MiniData(BigInteger))
        return std::make_unique<HexValue>(num);
    } else if (type == Value::VALUE_SCRIPT) {
        auto* cval = static_cast<StringValue*>(val);
        return std::make_unique<HexValue>(cval->getBytes());
    }

    throw ExecutionException(std::string("Invalid Type in HEX cast ") + std::to_string(type));
}

int HEX::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> HEX::getNewFunction() {
    return std::make_unique<HEX>();
}

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org