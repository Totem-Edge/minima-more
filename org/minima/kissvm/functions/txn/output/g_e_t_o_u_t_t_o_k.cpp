#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_t_o_k.hpp"

#include <memory>
#include <string>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No OS-specific behavior required for this file currently.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace output {

GETOUTTOK::GETOUTTOK()
    : org::minima::kissvm::functions::MinimaFunction("GETOUTTOK") {
}

std::unique_ptr<org::minima::kissvm::values::Value>
GETOUTTOK::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure correct parameter count
    checkExactParamNumber(requiredParams());

    // Which Output - must be from 0-255
    int output = zContract.getNumberParam(0, *this)->getNumber().getAsInt();

    // Get the Transaction
    org::minima::objects::Transaction& trans = zContract.getTransaction();

    // Check output exists..
    const std::vector<std::unique_ptr<org::minima::objects::Coin>>& outs = trans.getAllOutputs();
    if (output < 0 || static_cast<std::size_t>(output) >= outs.size()) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Output out of range " + std::to_string(output) + "/" + std::to_string(outs.size()));
    }

    // Get it..
    const org::minima::objects::Coin* cc = outs[output].get();

    // Return the token id
    return std::make_unique<org::minima::kissvm::values::HexValue>(cc->getTokenID());
}

int GETOUTTOK::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> GETOUTTOK::getNewFunction() {
    return std::make_unique<GETOUTTOK>();
}

} // namespace output
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org