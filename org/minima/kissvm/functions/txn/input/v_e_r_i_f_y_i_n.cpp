#include "org/minima/kissvm/functions/txn/input/v_e_r_i_f_y_i_n.hpp"

#include <vector>
#include <sstream>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

//
// FIX: Add missing headers for value types
//
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No OS-specific code needed; placeholder for potential platform divergences.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::BooleanValue;

using org::minima::objects::Transaction;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

VERIFYIN::VERIFYIN() : MinimaFunction("VERIFYIN") {}

std::unique_ptr<Value> VERIFYIN::runFunction(Contract& zContract) {
    // Check exact parameter count
    checkExactParamNumber(requiredParams());

    //
    // FIX 1: Use '->' operator on the std::unique_ptr
    //
    int input = zContract.getNumberParam(0, *this)->getNumber().getAsInt();

    //
    // FIX 2: Use '->' operator on the std::unique_ptr
    //
    MiniData address(zContract.getHexParam(1, *this)->getRawData());
    //
    // FIX 3: Use '->' operator on the std::unique_ptr
    //
    MiniNumber amount = zContract.getNumberParam(2, *this)->getNumber();
    //
    // FIX 4: Use '->' operator on the std::unique_ptr
    //
    MiniData tokenid(zContract.getHexParam(3, *this)->getRawData());

    // Access the transaction
    Transaction& trans = zContract.getTransaction();

    // Check the input exists
    auto& ins = trans.getAllInputs();
    if (input < 0 || static_cast<std::size_t>(input) >= ins.size()) {
        std::ostringstream oss;
        oss << "Input number out of range " << input << "/" << ins.size();
        throw ExecutionException(oss.str());
    }

    // Get the Coin
    Coin* cc = ins[input].get();

    // Address and token checks
    bool addr_ok = address.isEqual(cc->getAddress());
    bool tok_ok  = tokenid.isEqual(cc->getTokenID());

    // Amount (may be scaled if token)
    MiniNumber inamt = cc->getAmount();

    if (!cc->getTokenID().isEqual(Token::TOKENID_MINIMA)) {
        Token* td = cc->getToken();
        if (td == nullptr) {
            std::ostringstream oss;
            oss << "No token specified @ Input coin " << input << " " << cc->getTokenID().toString();
            throw ExecutionException(oss.str());
        }
        auto scaled = td->getScaledTokenAmount(cc->getAmount());
        inamt = *scaled;
    }

    bool amt_ok = inamt.isEqual(amount);

    // Return TRUE only if all are true
    return std::make_unique<BooleanValue>(addr_ok && amt_ok && tok_ok);
}

int VERIFYIN::requiredParams() {
    return 4;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> VERIFYIN::getNewFunction() {
    return std::make_unique<VERIFYIN>();
}

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
