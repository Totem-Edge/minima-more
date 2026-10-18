#include "org/minima/kissvm/functions/string/s_u_b_s_t_r.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

SUBSTR::SUBSTR() : MinimaFunction("SUBSTR") {}

std::unique_ptr<org::minima::kissvm::values::Value>
SUBSTR::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure exact parameter count
    checkExactParamNumber(requiredParams());

    // Get start and end indices
    auto startNum = zContract.getNumberParam(0, *this);
    auto endNum   = zContract.getNumberParam(1, *this);

    int start = startNum->getNumber().getAsInt();
    int end   = endNum->getNumber().getAsInt();

    int len = end - start;
    if (len < 0) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            std::string("Negative SUBSTR length ") + std::to_string(len));
    }

    // Get the string value
    auto strVal = zContract.getStringParam(2, *this);
    std::string main = strVal->toString();

    // Bounds check
    if (start < 0 || end > static_cast<int>(main.length())) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            std::string("SUBSTR range outside size of String ")
            + std::to_string(start) + "-" + std::to_string(end)
            + " length:" + std::to_string(main.length()));
    }

    // Extract substring [start, end)
    std::string substr = main.substr(static_cast<std::size_t>(start),
                                     static_cast<std::size_t>(len));

    return std::make_unique<org::minima::kissvm::values::StringValue>(substr);
}

std::unique_ptr<MinimaFunction> SUBSTR::getNewFunction() {
    return std::make_unique<SUBSTR>();
}

int SUBSTR::requiredParams() {
    return 3;
}

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org