#include "org/minima/kissvm/functions/hex/c_o_n_c_a_t.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::expressions::Expression;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::Value;

CONCAT::CONCAT() : MinimaFunction("CONCAT") {}

std::unique_ptr<Value> CONCAT::runFunction(Contract& zContract) {
    // Check parameters..
    checkMinParamNumber(requiredParams());

    // Run through the function parameters and concatenate..
    const auto& params = getAllParameters();

    // Prepare storage for parameter byte arrays
    std::vector<std::vector<std::uint8_t>> parambytes;
    parambytes.resize(params.size());

    int totlen = 0;
    int counter = 0;

    for (const auto& p : params) {
        Expression* exp = p.get();
        Value* vv = exp->getValue(zContract);
        checkIsOfType(*vv, Value::VALUE_HEX);

        // This is a HexValue
        HexValue* hex = static_cast<HexValue*>(vv);

        // Get the bytes
        parambytes[counter] = hex->getRawData();
        totlen += static_cast<int>(parambytes[counter].size());

        // 1MB max size..
        if (totlen > Contract::MAX_DATA_SIZE) {
            throw ExecutionException(
                "MAX HEX value size reached : " + std::to_string(totlen) + "/" + std::to_string(Contract::MAX_DATA_SIZE));
        }

        counter++;
    }

    // The result is placed in here
    std::vector<std::uint8_t> result;
    result.resize(static_cast<std::size_t>(totlen));

    // And sum
    int pos = 0;
    for (int i = 0; i < counter; ++i) {
        const auto& src = parambytes[i];
        if (!src.empty()) {
            std::copy(src.begin(), src.end(), result.begin() + pos);
            pos += static_cast<int>(src.size());
        }
    }

    return std::make_unique<HexValue>(result);
}

bool CONCAT::isRequiredMinimumParameterNumber() const {
    return true;
}

int CONCAT::requiredParams() {
    return 2;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> CONCAT::getNewFunction() {
    return std::make_unique<CONCAT>();
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org