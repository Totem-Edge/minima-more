#include "org/minima/kissvm/functions/sha/s_h_a2.hpp"

#include <vector>
#include <cstdint>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/utils/crypto.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace sha {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::StringValue;
using org::minima::utils::Crypto;

SHA2::SHA2() : MinimaFunction("SHA2") {}

std::unique_ptr<Value> SHA2::runFunction(Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Evaluate first parameter
    auto& expr0 = getParameter(0);
    Value* vv = expr0.getValue(zContract);

    // Must be HEX or SCRIPT
    checkIsOfType(*vv, Value::VALUE_HEX | Value::VALUE_SCRIPT);

    std::vector<std::uint8_t> data;
    if (vv->getValueType() == Value::VALUE_HEX) {
        auto* hex = dynamic_cast<HexValue*>(vv);
        if (!hex) {
            throw ExecutionException("Internal error: Expected HexValue after type check");
        }
        data = hex->getRawData();
    } else {
        auto* str = dynamic_cast<StringValue*>(vv);
        if (!str) {
            throw ExecutionException("Internal error: Expected StringValue after type check");
        }
        data = str->getBytes();
    }

    // Perform SHA2 (SHA-256) hashing
    std::vector<std::uint8_t> ans = Crypto::getInstance().hashSHA2(data);

    // Return as HexValue
    return std::make_unique<HexValue>(ans);
}

int SHA2::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SHA2::getNewFunction() {
    return std::make_unique<SHA2>();
}

} // namespace sha
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org