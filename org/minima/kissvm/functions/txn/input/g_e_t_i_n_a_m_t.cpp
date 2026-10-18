#include "org/minima/kissvm/functions/txn/input/g_e_t_i_n_a_m_t.hpp"

#include <vector>
#include <sstream>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

GETINAMT::GETINAMT()
    : org::minima::kissvm::functions::MinimaFunction("GETINAMT") {}

std::unique_ptr<org::minima::kissvm::values::Value>
GETINAMT::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure parameter count is exactly as required
    checkExactParamNumber(requiredParams());

    // Which input
    int inputIndex = zContract.getNumberParam(0, *this)->getNumber().getAsInt();

    // Get the transaction
    org::minima::objects::Transaction& trans = zContract.getTransaction();

    // Get inputs and bounds check
    auto& inputs = trans.getAllInputs();
    if (inputIndex < 0 || static_cast<std::size_t>(inputIndex) >= inputs.size()) {
        std::ostringstream oss;
        oss << "Input number out of range " << inputIndex << "/" << inputs.size();
        throw org::minima::kissvm::exceptions::ExecutionException(oss.str());
    }

    // Access the specified input coin
    org::minima::objects::Coin* cc = inputs[inputIndex].get();

    // Is it a token?
    if (!cc->getTokenID().isEqual(org::minima::objects::Token::TOKENID_MINIMA)) {
        // Retrieve the token details
        const org::minima::objects::Token* td = cc->getToken();
        if (td == nullptr) {
            std::ostringstream oss;
            // Java prints cc.getToken() which is null here; replicate that intent
            oss << "No Token for Input Coin @ " << inputIndex << " null";
            throw org::minima::kissvm::exceptions::ExecutionException(oss.str());
        }

        // Return the scaled token amount
        auto scaled = td->getScaledTokenAmount(cc->getAmount());
        return std::make_unique<org::minima::kissvm::values::NumberValue>(*scaled);
    }

    // Plain Minima amount
    return std::make_unique<org::minima::kissvm::values::NumberValue>(cc->getAmount());
}

int GETINAMT::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> GETINAMT::getNewFunction() {
    return std::make_unique<GETINAMT>();
}

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org