#include "org/minima/kissvm/functions/txn/output/v_e_r_i_f_y_o_u_t.hpp"

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
// No OS-specific functionality required; placeholder to show cross-platform awareness.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace output {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::Transaction;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

VERIFYOUT::VERIFYOUT()
    : org::minima::kissvm::functions::MinimaFunction("VERIFYOUT") {}

std::unique_ptr<Value> VERIFYOUT::runFunction(Contract& zContract) {
    // Check parameters count: must be 4 or 5
    int paramnum = static_cast<int>(getAllParameters().size());
    if (paramnum < 4 || paramnum > 5) {
        throw ExecutionException("VERIFYOUT requires 4 or 5 parameters");
    }

    //
    // FIX 1: Use '->' operator on the std::unique_ptr
    //
    int output = zContract.getNumberParam(0, *this)->getNumber().getAsInt();

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

    // Are we checking KEEPSTATE
    bool checkkeepstate = false;
    bool keepstate = false;
    if (paramnum == 5) {
        checkkeepstate = true;
        //
        // FIX 5: Use '->' operator on the std::unique_ptr
        //
        keepstate = zContract.getBoolParam(4, *this)->isTrue();
    }

    // Get the transaction
    Transaction& trans = zContract.getTransaction();

    // Check output exists
    auto& outs = trans.getAllOutputs();
    if (output < 0 || static_cast<std::size_t>(output) >= outs.size()) {
        throw ExecutionException(
            std::string("Output out of range ") + std::to_string(output) + "/" + std::to_string(outs.size()));
    }

    // Get it
    Coin* cc = outs[static_cast<std::size_t>(output)].get();

    // Check Keep State
    bool samestate = true;
    if (checkkeepstate) {
        samestate = (cc->storeState() == keepstate);
    }

    // Now Check basic fields
    bool addr = address.isEqual(cc->getAddress());
    bool tok  = tokenid.isEqual(cc->getTokenID());

    // The amount may need to be scaled
    MiniNumber outamt = cc->getAmount();

    // Could be a token Amount!
    if (!cc->getTokenID().isEqual(Token::TOKENID_MINIMA)) {
        // Get the token details
        Token* cctok = cc->getToken();
        if (cctok == nullptr) {
            throw ExecutionException(
                std::string("No token specified @ Output coin ") + std::to_string(output) + " " + cc->getTokenID().to0xString());
        }

        // Scale the amount
        std::unique_ptr<MiniNumber> scaled = cctok->getScaledTokenAmount(cc->getAmount());
        outamt = *scaled;
    }

    // Are they equal
    bool amt = outamt.isEqual(amount);

    // If all equal..
    bool ver = addr && amt && tok && samestate;

    // Log the error
    if (!ver) {
        std::ostringstream oss;
        oss << "VERIFYOUT failed @ ouptut " << output
            << " found (address:" << cc->getAddress().to0xString()
            << " amount:" << outamt.toString()
            << " tokenid:" << cc->getTokenID().to0xString()
            << " keepstate:" << (cc->storeState() ? "true" : "false") << " ) "
            << "expected (address:" << address.to0xString()
            << " amount:" << amount.toString()
            << " tokenid:" << tokenid.to0xString()
            << " keepstate:" << (keepstate ? "true" : "false") << ") "
            << "CheckKeepState:" << (checkkeepstate ? "true" : "false");
        zContract.traceLog(oss.str());
    }

    // Return if all true
    return std::make_unique<BooleanValue>(ver);
}

bool VERIFYOUT::isRequiredMinimumParameterNumber() const {
    return true;
}

int VERIFYOUT::requiredParams() {
    return 4;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> VERIFYOUT::getNewFunction() {
    return std::make_unique<VERIFYOUT>();
}

} // namespace output
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
