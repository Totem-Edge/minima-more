#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_a_d_d_r.hpp"

#include <sstream>
#ifdef _WIN32
// No OS-specific behavior required; placeholder for potential platform differences.
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

GETINADDR::GETINADDR()
    : org::minima::kissvm::functions::MinimaFunction("GETINADDR") {}

std::unique_ptr<org::minima::kissvm::values::Value>
GETINADDR::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Which input
    int inputIndex = 0;
    {
        auto num = zContract.getNumberParam(0, *this);
        inputIndex = num->getNumber().getAsInt();
    }

    // Get the transaction
    org::minima::objects::Transaction& trans = zContract.getTransaction();

    // Access inputs and check bounds
    const auto& ins = trans.getAllInputs();
    if (inputIndex < 0 || static_cast<std::size_t>(inputIndex) >= ins.size()) {
        std::ostringstream oss;
        oss << "Input number out of range " << inputIndex << "/" << ins.size();
        throw org::minima::kissvm::exceptions::ExecutionException(oss.str());
    }

    // Get the coin
    const org::minima::objects::Coin* cc = ins[inputIndex].get();

    // Return the address as HexValue
    return std::make_unique<org::minima::kissvm::values::HexValue>(cc->getAddress());
}

int GETINADDR::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction>
GETINADDR::getNewFunction() {
    return std::make_unique<GETINADDR>();
}

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org