#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
namespace functions {

class MULTISIG final : public MinimaFunction {
public:
    MULTISIG();

    // Execute and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Factory for a new copy of this function
    std::unique_ptr<MinimaFunction> getNewFunction() override;

    // How many parameters are required (minimum in this case)
    int requiredParams() override;

    // This function uses a minimum required parameter count
    bool isRequiredMinimumParameterNumber() const override;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org