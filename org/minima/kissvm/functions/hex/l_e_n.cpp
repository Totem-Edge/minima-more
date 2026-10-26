#include "org/minima/kissvm/functions/hex/l_e_n.hpp"

#include <vector>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/utils/utf16.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

using org::minima::kissvm::Contract;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::StringValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::exceptions::ExecutionException;

LEN::LEN()
    : MinimaFunction("LEN") {
}

std::unique_ptr<Value> LEN::runFunction(Contract& zContract) {
    // Ensure exactly one parameter
    checkExactParamNumber(requiredParams());

    // Evaluate the first parameter (non-owning pointer)
    Value* val = getParameter(0).getValue(zContract);
    if (!val) {
        throw ExecutionException("LEN internal error: null parameter value");
    }

    const int vtype = val->getValueType();
    if (vtype == Value::VALUE_HEX) {
        // Cast and compute byte length
        HexValue* hv = dynamic_cast<HexValue*>(val);
        if (!hv) {
            throw ExecutionException("Internal type error in LEN for HEX parameter");
        }
        int len = static_cast<int>(hv->getRawData().size());
        return std::make_unique<NumberValue>(len);

    } else if (vtype == Value::VALUE_SCRIPT) {
        // Cast and compute string length
        StringValue* sv = dynamic_cast<StringValue*>(val);
        if (!sv) {
            throw ExecutionException("Internal type error in LEN for STRING parameter");
        }
        // Java String.length() counts UTF-16 code units, not UTF-8 bytes.
        const std::u16string units = org::minima::utils::utf16::from_utf8(sv->toString());
        int len = static_cast<int>(units.size());
        return std::make_unique<NumberValue>(len);
    }

    // Error if neither HEX nor STRING
    throw ExecutionException(std::string("LEN requires HEX or STRING param @ ") + val->toString());
}

int LEN::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> LEN::getNewFunction() {
    return std::make_unique<LEN>();
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org