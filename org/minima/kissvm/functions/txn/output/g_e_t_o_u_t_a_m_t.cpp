#include "org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_a_m_t.hpp"

#include <sstream>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"

//
// FIX 1: Include the missing header
//
#include "org/minima/kissvm/values/number_value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace output {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::functions::MinimaFunction;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::Transaction;

GETOUTAMT::GETOUTAMT()
    : MinimaFunction("GETOUTAMT") {
}

std::unique_ptr<Value> GETOUTAMT::runFunction(Contract& zContract) {
    // Ensure correct number of params
    checkExactParamNumber(requiredParams());

    //
    // FIX 2: Use auto to declare the unique_ptr
    // FIX 3: Use the arrow '->' operator
    //
    auto output_val = zContract.getNumberParam(0, *this);
    int output = output_val->getNumber().getAsInt();

    // Get the Transaction
    Transaction& trans = zContract.getTransaction();

    // Check output exists
    const std::vector<std::unique_ptr<Coin>>& outs = trans.getAllOutputs();
    if (output < 0 || static_cast<size_t>(output) >= outs.size()) {
        std::ostringstream oss;
        oss << "Output out of range " << output << "/" << outs.size();
        throw ExecutionException(oss.str());
    }

    // Get it
    const Coin* cc = outs[static_cast<size_t>(output)].get();

    // Is it a Token..
    if (!cc->getTokenID().isEqual(Token::TOKENID_MINIMA)) {
        // Get the Token object
        const Token* td = cc->getToken();
        if (td == nullptr) {
            std::ostringstream oss;
            oss << "No Token for Output Coin @ " << output << " " << cc->getToken();
            throw ExecutionException(oss.str());
        }

        // Return the scaled amount
        auto scaled = td->getScaledTokenAmount(cc->getAmount());
        return std::make_unique<NumberValue>(*scaled);
    }

    // Return the Amount
    return std::make_unique<NumberValue>(cc->getAmount());
}

int GETOUTAMT::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> GETOUTAMT::getNewFunction() {
    return std::make_unique<GETOUTAMT>();
}

} // namespace output
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
