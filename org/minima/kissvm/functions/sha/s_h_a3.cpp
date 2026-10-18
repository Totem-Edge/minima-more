#include "org/minima/kissvm/functions/sha/s_h_a3.hpp"

#include <vector>
#include <cstdint>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/utils/crypto.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::StringValue;

SHA3::SHA3()
    : MinimaFunction("SHA3") {
}

std::unique_ptr<Value> SHA3::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure correct parameter count
    checkExactParamNumber(requiredParams());

    // Evaluate first parameter
    Value* valptr = getParameter(0).getValue(zContract);
    Value& vv = *valptr;

    // Must be HEX or SCRIPT
    checkIsOfType(vv, Value::VALUE_HEX | Value::VALUE_SCRIPT);

    // Extract bytes
    std::vector<std::uint8_t> data;
    if (vv.getValueType() == Value::VALUE_HEX) {
        // HEX
        auto* hex = dynamic_cast<HexValue*>(valptr);
        data = hex->getRawData();
    } else {
        // SCRIPT (string)
        auto* scr = dynamic_cast<StringValue*>(valptr);
        data = scr->getBytes();
    }

    // Perform the SHA3 operation (hashData is SHA3-256 by project convention)
    std::vector<std::uint8_t> ans = org::minima::utils::Crypto::getInstance().hashData(data);

    // Return the new HexValue
    return std::make_unique<HexValue>(ans);
}

int SHA3::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> SHA3::getNewFunction() {
    return std::make_unique<SHA3>();
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org