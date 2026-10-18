#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_k_e_e_p_s_t_a_t_e.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No OS-specific behavior required for this function.
// Placeholder for potential platform-specific adjustments.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::Coin;
using org::minima::objects::Transaction;

GETOUTKEEPSTATE::GETOUTKEEPSTATE()
    : MinimaFunction("GETOUTKEEPSTATE") {}

std::unique_ptr<Value> GETOUTKEEPSTATE::runFunction(Contract& zContract) {
    // Ensure the exact number of parameters
    checkExactParamNumber(requiredParams());

    // Which Output - must be from 0-255 (range enforced by bounds check below)
    std::unique_ptr<NumberValue> num = zContract.getNumberParam(0, *this);
    int output = num->getNumber().getAsInt();

    // Get the Transaction (reference)
    Transaction& trans = zContract.getTransaction();

    // Check output exists..
    const std::vector<std::unique_ptr<Coin>>& outs = trans.getAllOutputs();
    if (output < 0 || outs.size() <= static_cast<std::size_t>(output)) {
        throw ExecutionException(
            std::string("Output out of range ") +
            std::to_string(output) + "/" + std::to_string(outs.size()));
        }

    // Get it..
    const Coin* cc = outs[static_cast<std::size_t>(output)].get();

    // Return the storeState flag as a BooleanValue
    return std::make_unique<BooleanValue>(cc->storeState());
}

int GETOUTKEEPSTATE::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> GETOUTKEEPSTATE::getNewFunction() {
    return std::make_unique<GETOUTKEEPSTATE>();
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org