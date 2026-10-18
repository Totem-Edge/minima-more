#include "org/minima/kissvm/functions/cast/n_u_m_b_e_r.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::StringValue;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

NUMBER::NUMBER() : MinimaFunction("NUMBER") {}

std::unique_ptr<Value> NUMBER::runFunction(Contract& zContract) {
    // Ensure exactly one parameter
    checkExactParamNumber(requiredParams());

    // Evaluate parameter expression
    auto val = getParameter(0).getValue(zContract);
    int type = val->getValueType();

    if (type == Value::VALUE_BOOLEAN) {
        const auto& cval = static_cast<const BooleanValue&>(*val);
        if (cval.isTrue()) {
            return std::make_unique<NumberValue>(1);
        } else {
            return std::make_unique<NumberValue>(0);
        }
    } else if (type == Value::VALUE_HEX) {
        const auto& cval = static_cast<const HexValue&>(*val);
        const MiniData& md1 = cval.getMiniData();
        // Construct number from decimal string representation of the data
        MiniNumber num(md1.getDataValue());
        return std::make_unique<NumberValue>(num);
    } else if (type == Value::VALUE_SCRIPT) {
        const auto& cval = static_cast<const StringValue&>(*val);
        return std::make_unique<NumberValue>(cval.toString());
    } else if (type == Value::VALUE_NUMBER) {
        const auto& cval = static_cast<const NumberValue&>(*val);
        return std::make_unique<NumberValue>(cval.getNumber());
    }

    throw ExecutionException(std::string("Invalid Type in NUMBER cast ") + std::to_string(type));
}

int NUMBER::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> NUMBER::getNewFunction() {
    return std::make_unique<NUMBER>();
}

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org