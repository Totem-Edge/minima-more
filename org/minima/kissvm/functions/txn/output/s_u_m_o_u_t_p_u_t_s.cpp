#include "org/minima/kissvm/functions/txn/output/s_u_m_o_u_t_p_u_t_s.hpp"

#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

//
// FIX 2: Add missing header for HexValue
//
#include "org/minima/kissvm/values/hex_value.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace output {

using org::minima::kissvm::Contract;
using org::minima::kissvm::values::NumberValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::Transaction;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

SUMOUTPUTS::SUMOUTPUTS()
    : MinimaFunction("SUMOUTPUTS") {
}

std::unique_ptr<Value> SUMOUTPUTS::runFunction(Contract& zContract) {
    checkExactParamNumber(requiredParams());

    //
    // FIX 1: Use '->' operator on the std::unique_ptr
    //
    MiniData tokenid = zContract.getHexParam(0, *this)->getMiniData();

    // Get the Transaction
    Transaction& trans = zContract.getTransaction();

    // The Total
    MiniNumber total = MiniNumber::ZERO();

    // Cycle through the outputs
    const auto& outputs = trans.getAllOutputs();
    for (const std::unique_ptr<Coin>& ccp : outputs) {
        const Coin* cc = ccp.get();
        if (!cc) {
            continue;
        }

        if (cc->getTokenID().isEqual(tokenid)) {
            if (tokenid.isEqual(Token::TOKENID_MINIMA)) {
                // Plain Minima
                total = total.add(cc->getAmount());
            } else {
                // Get the token
                const Token* td = cc->getToken();

                // Add the scaled amount
                // Note: getScaledTokenAmount returns a unique_ptr<MiniNumber>
                std::unique_ptr<MiniNumber> scaled = td->getScaledTokenAmount(cc->getAmount());
                total = total.add(*scaled);
            }
        }
    }

    // Return the Amount
    return std::make_unique<NumberValue>(total);
}

int SUMOUTPUTS::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SUMOUTPUTS::getNewFunction() {
    return std::make_unique<SUMOUTPUTS>();
}

} // namespace output
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
