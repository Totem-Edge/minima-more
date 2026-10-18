#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; class NumberValue; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

class SQRT final : public org::minima::kissvm::functions::MinimaFunction {
public:
    SQRT();

    // Execute and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // How many parameters are required
    int requiredParams() override;

    // Factory for a new copy of this function
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org