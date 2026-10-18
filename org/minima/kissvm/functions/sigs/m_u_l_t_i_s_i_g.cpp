#include "org/minima/kissvm/functions/sigs/m_u_l_t_i_s_i_g.hpp"

#include <memory>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

#ifdef _WIN32
// No OS-specific behavior needed for this function
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

using org::minima::kissvm::Contract;
using org::minima::kissvm::exceptions::ExecutionException;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::HexValue;

MULTISIG::MULTISIG() : MinimaFunction("MULTISIG") {}

std::unique_ptr<org::minima::kissvm::values::Value>
MULTISIG::runFunction(Contract& zContract) {
    // Ensure at least required minimum parameters
    checkMinParamNumber(requiredParams());

    // How many required
    auto numparam = zContract.getNumberParam(0, *this);
    int num = numparam->getNumber().getAsInt();

    // How many to check from
    int tot = getParameterNum() - 1;

    // Check valid request
    if (num < 0) {
        throw ExecutionException("CANNOT check negative sigs in MULTISIG " + std::to_string(num));
    }

    // Cycle through signatures
    int found = 0;
    for (int i = 0; i < tot; ++i) {
        auto sig = zContract.getHexParam(1 + i, *this);

        if (zContract.checkSignature(*sig)) {
            ++found;
        }

        if (found >= num) {
            break;
        }
    }

    // Return TRUE if enough signatures found, else FALSE
    if (found >= num) {
        return std::make_unique<BooleanValue>(true);
    } else {
        return std::make_unique<BooleanValue>(false);
    }
}

bool MULTISIG::isRequiredMinimumParameterNumber() const {
    return true;
}

int MULTISIG::requiredParams() {
    return 2;
}

std::unique_ptr<MinimaFunction> MULTISIG::getNewFunction() {
    return std::make_unique<MULTISIG>();
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org