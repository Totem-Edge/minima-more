#include "org/minima/kissvm/functions/state/s_a_m_e_s_t_a_t_e.hpp"

#include <memory>
#include <string>
#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace state {

SAMESTATE::SAMESTATE()
    : org::minima::kissvm::functions::MinimaFunction("SAMESTATE") {}

std::unique_ptr<org::minima::kissvm::values::Value>
SAMESTATE::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure correct number of parameters
    checkExactParamNumber(requiredParams());

    // Get start and end indices
    auto startnv = zContract.getNumberParam(0, *this);
    auto endnv   = zContract.getNumberParam(1, *this);

    int start = startnv->getNumber().getAsInt();
    int end   = endnv->getNumber().getAsInt();

    // Simple checks
    if (start < 0 || end < start) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            "Invalid range check for SAMESTATE " + std::to_string(start) + " " + std::to_string(end));
    }

    // Now check the old state and the current state are the same
    for (int i = start; i <= end; ++i) {
        // Get the old state and current state as strings
        std::unique_ptr<org::minima::kissvm::values::Value> oldv = zContract.getPrevState(i);
        std::unique_ptr<org::minima::kissvm::values::Value> newv = zContract.getState(i);

        std::string olds = oldv->toString();
        std::string news = newv->toString();

        // check the same
        if (olds != news) {
            zContract.traceLog("SAMESTATE FAIL [" + std::to_string(i) + "] PREV:" + olds + " / CURRENT:" + news);
            return std::make_unique<org::minima::kissvm::values::BooleanValue>(false);
        }
    }

    return std::make_unique<org::minima::kissvm::values::BooleanValue>(true);
}

int SAMESTATE::requiredParams() {
    return 2;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SAMESTATE::getNewFunction() {
    return std::make_unique<SAMESTATE>();
}

} // namespace state
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org