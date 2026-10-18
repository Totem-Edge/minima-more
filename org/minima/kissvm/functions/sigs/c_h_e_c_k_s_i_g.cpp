#include "org/minima/kissvm/functions/sigs/c_h_e_c_k_s_i_g.hpp"

#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"

#ifdef _WIN32
// No OS-specific behavior required; placeholder for potential future use.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace sigs {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::HexValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::base::MiniData;
using org::minima::objects::keys::Signature;
using org::minima::objects::keys::TreeKey;

CHECKSIG::CHECKSIG()
    : MinimaFunction("CHECKSIG") {
}

std::unique_ptr<Value> CHECKSIG::runFunction(Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // This function costs more CPU cycles
    zContract.incrementInstructions(31);

    //
    // FIX 1: Change 'HexValue' to 'auto' to capture the std::unique_ptr
    //
    auto pubkey = zContract.getHexParam(0, *this);
    auto data   = zContract.getHexParam(1, *this);
    auto sig    = zContract.getHexParam(2, *this);

    //
    // FIX 2: Use '->' operator on smart pointers
    //
    const MiniData& pubk = pubkey->getMiniData();

    // Simple checks
    //
    // FIX 3: Use '->' operator on smart pointer
    //
    if (pubk.getLength() == 0 || sig->getMiniData().getLength() == 0) {
        throw ExecutionException("Invalid ZERO length params for CHECKSIG");
    }

    // Create a TreeKey and set the public key
    TreeKey checker;
    checker.setPublicKey(pubk);

    //
    // FIX 4: Use '->' operator on smart pointer
    //
    std::unique_ptr<Signature> signature = Signature::convertMiniDataVersion(sig->getMiniData());

    //
    // FIX 5: Use '->' operator on smart pointer
    //
    const MiniData& sigdata = data->getMiniData();

    // Verify; if signature failed to convert, treat as invalid
    bool ok = false;
    if (signature) {
        ok = checker.verify(sigdata, *signature);
    } else {
        ok = false;
    }

    return std::make_unique<BooleanValue>(ok);
}

int CHECKSIG::requiredParams() {
    return 3;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> CHECKSIG::getNewFunction() {
    return std::make_unique<CHECKSIG>();
}

} // namespace sigs
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org
