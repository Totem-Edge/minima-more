#include "org/minima/kissvm/functions/state/p_r_e_v_s_t_a_t_e.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No OS-specific behavior required here; placeholder for potential future use.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace state {

PREVSTATE::PREVSTATE()
    : org::minima::kissvm::functions::MinimaFunction("PREVSTATE") {
}

std::unique_ptr<org::minima::kissvm::values::Value>
PREVSTATE::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Which state number
    auto num = zContract.getNumberParam(0, *this);
    int statenum = num->getNumber().getAsInt();

    // Work it out
    return zContract.getPrevState(statenum);
}

int PREVSTATE::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction>
PREVSTATE::getNewFunction() {
    return std::make_unique<PREVSTATE>();
}

} // namespace state
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org