#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace state {

class PREVSTATE final : public org::minima::kissvm::functions::MinimaFunction {
public:
    PREVSTATE();

    // Execute and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Factory for a new copy of this function
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;

    // How many parameters are required
    int requiredParams() override;
};

} // namespace state
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org