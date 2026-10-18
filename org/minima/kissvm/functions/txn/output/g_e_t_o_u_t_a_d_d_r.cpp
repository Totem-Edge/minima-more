#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_a_d_d_r.hpp"

#include <limits>
#include <stdexcept>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

GETOUTADDR::GETOUTADDR() : MinimaFunction("GETOUTADDR") {}

std::unique_ptr<org::minima::kissvm::values::Value>
GETOUTADDR::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Which Output - must be from 0-255 (Java semantics)
    auto numParam = zContract.getNumberParam(0, *this);

    // Convert to int robustly via string parsing to avoid relying on specific MiniNumber APIs
    int outputIndex = 0;
    {
        const std::string sval = numParam->toString();
        long long llval = 0;
        try {
            llval = std::stoll(sval);
        } catch (const std::exception&) {
            throw org::minima::kissvm::exceptions::ExecutionException(
                "Invalid numerical parameter for GETOUTADDR: " + sval);
        }
        if (llval < std::numeric_limits<int>::min() || llval > std::numeric_limits<int>::max()) {
            throw org::minima::kissvm::exceptions::ExecutionException(
                "Number parameter out of int range");
        }
        outputIndex = static_cast<int>(llval);
    }

    // Get the Transaction
    org::minima::objects::Transaction& trans = zContract.getTransaction();

    // Check output exists..
    const std::vector<std::unique_ptr<org::minima::objects::Coin>>& outs = trans.getAllOutputs();
    if (outputIndex < 0 || static_cast<std::size_t>(outputIndex) >= outs.size()) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Output out of range " + std::to_string(outputIndex) + "/" + std::to_string(outs.size()));
    }

    // Get it..
    const org::minima::objects::Coin* cc = outs[outputIndex].get();

    // Return the address
    return std::make_unique<org::minima::kissvm::values::HexValue>(cc->getAddress());
}

std::unique_ptr<MinimaFunction> GETOUTADDR::getNewFunction() {
    return std::make_unique<GETOUTADDR>();
}

int GETOUTADDR::requiredParams() {
    return 1;
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org