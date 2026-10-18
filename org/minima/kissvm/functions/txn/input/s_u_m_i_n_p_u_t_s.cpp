#include "org/minima/kissvm/functions/txn/input/s_u_m_i_n_p_u_t_s.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No OS-specific behavior required for this function.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::NumberValue;
using org::minima::objects::Transaction;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

SUMINPUTS::SUMINPUTS()
    : MinimaFunction("SUMINPUTS") {
}

std::unique_ptr<Value> SUMINPUTS::runFunction(org::minima::kissvm::Contract& zContract) {
    checkExactParamNumber(requiredParams());

    //
    // FIX: Use '->' operator on the std::unique_ptr
    //
    MiniData tokenid = zContract.getHexParam(0, *this)->getMiniData();

    // Get the Transaction
    Transaction& trans = zContract.getTransaction();

    // The Total
    MiniNumber total = MiniNumber::ZERO();

    // Cycle through the inputs
    const auto& inputs = trans.getAllInputs();
    for (const auto& uptr : inputs) {
        const Coin* cc = uptr.get();
        if (!cc) {
            continue; // Defensive: skip null entries if any
        }

        if (cc->getTokenID().isEqual(tokenid)) {

            if (tokenid.isEqual(Token::TOKENID_MINIMA)) {
                // Plain Minima
                total = total.add(cc->getAmount());
            } else {
                // Get the token..
                const Token* td = cc->getToken();

                // Add the scaled amount..
                // Java code assumes td is non-null when tokenID != MINIMA.
                // We mirror that assumption; if td is nullptr, behavior is undefined in Java (NPE).
                if (td) {
                    std::unique_ptr<MiniNumber> scaled = td->getScaledTokenAmount(cc->getAmount());
                    total = total.add(*scaled);
                } else {
                    // Defensive: If token is unexpectedly null, do nothing (avoid crash).
                    // This keeps behavior safe without inventing new exception paths.
                }
            }
        }
    }

    // Return the Amount
    return std::make_unique<NumberValue>(total);
}

int SUMINPUTS::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SUMINPUTS::getNewFunction() {
    return std::make_unique<SUMINPUTS>();
}

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
