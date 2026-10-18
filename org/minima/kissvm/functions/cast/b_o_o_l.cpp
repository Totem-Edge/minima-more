#include "org/minima/kissvm/functions/cast/b_o_o_l.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::StringValue;
using org::minima::objects::base::MiniNumber;

BOOL::BOOL()
    : org::minima::kissvm::functions::MinimaFunction("BOOL") {
}

std::unique_ptr<Value> BOOL::runFunction(org::minima::kissvm::Contract& zContract) {
    checkExactParamNumber(requiredParams());

    // Evaluate the parameter
    Value* val = getParameter(0).getValue(zContract);

    bool ret = false;
    int type = val->getValueType();

    if (type == Value::VALUE_BOOLEAN) {
        auto* cval = dynamic_cast<BooleanValue*>(val);
        if (!cval) {
            throw org::minima::kissvm::exceptions::ExecutionException("Type mismatch casting to BooleanValue");
        }
        ret = cval->isTrue();

    } else if (type == Value::VALUE_HEX) {
        auto* cval = dynamic_cast<HexValue*>(val);
        if (!cval) {
            throw org::minima::kissvm::exceptions::ExecutionException("Type mismatch casting to HexValue");
        }

        // Convert to a MiniNumber to ensure it's not too big a data structure
        const auto& md = cval->getMiniData();
        MiniNumber num(md.getDataValue());

        // 0 is FALSE
        ret = !num.isEqual(MiniNumber::ZERO());

    } else if (type == Value::VALUE_NUMBER) {
        auto* cval = dynamic_cast<NumberValue*>(val);
        if (!cval) {
            throw org::minima::kissvm::exceptions::ExecutionException("Type mismatch casting to NumberValue");
        }

        // 0 is FALSE
        ret = !cval->getNumber().isEqual(MiniNumber::ZERO());

    } else if (type == Value::VALUE_SCRIPT) {
        auto* cval = dynamic_cast<StringValue*>(val);
        if (!cval) {
            throw org::minima::kissvm::exceptions::ExecutionException("Type mismatch casting to StringValue");
        }

        // check for "FALSE" - everything else is TRUE
        ret = !(cval->toString() == std::string("FALSE"));

    } else {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Invalid Type in BOOL cast " + std::to_string(type));
    }

    return std::make_unique<BooleanValue>(ret);
}

int BOOL::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> BOOL::getNewFunction() {
    return std::make_unique<BOOL>();
}

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org