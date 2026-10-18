#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

class GETOUTKEEPSTATE final : public MinimaFunction {
public:
    GETOUTKEEPSTATE();

    // Execute and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // How many parameters are required
    int requiredParams() override;

    // Factory for a new copy of this function
    std::unique_ptr<MinimaFunction> getNewFunction() override;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org