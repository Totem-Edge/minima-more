#include "org/minima/kissvm/functions/state/s_t_a_t_e.hpp"

#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#ifdef _WIN32
// No OS-specific behavior required for this class.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace state {

STATE::STATE() : org::minima::kissvm::functions::MinimaFunction("STATE") {}

std::unique_ptr<org::minima::kissvm::values::Value>
STATE::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure correct number of parameters
    checkExactParamNumber(requiredParams());

    // Which state index
    std::unique_ptr<org::minima::kissvm::values::NumberValue> num =
        zContract.getNumberParam(0, *this);
    int statenum = num->getNumber().getAsInt();

    // Retrieve and return the state value
    return zContract.getState(statenum);
}

int STATE::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> STATE::getNewFunction() {
    return std::make_unique<STATE>();
}

} // namespace state
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org