#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_t_o_k.hpp"

#include <vector>
#include <string>
#include <memory>

#ifdef _WIN32
// No OS-specific code required; placeholder for potential differences
#endif

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::Value;

GETINTOK::GETINTOK()
    : MinimaFunction("GETINTOK") {}

std::unique_ptr<Value> GETINTOK::runFunction(Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Which input index
    int input =
        zContract.getNumberParam(0, *this)->getNumber().getAsInt();

    // Get the Transaction
    org::minima::objects::Transaction& trans = zContract.getTransaction();

    // Check input exists
    auto& ins = trans.getAllInputs();
    if (input < 0 || static_cast<std::size_t>(input) >= ins.size()) {
        throw ExecutionException(
            "Input number out of range " + std::to_string(input) + "/" + std::to_string(ins.size()));
    }

    // Get it
    org::minima::objects::Coin* cc = ins[input].get();

    // Return the token id as HexValue
    return std::make_unique<HexValue>(cc->getTokenID());
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> GETINTOK::getNewFunction() {
    return std::make_unique<GETINTOK>();
}

int GETINTOK::requiredParams() {
    return 1;
}

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org