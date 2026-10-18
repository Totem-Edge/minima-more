#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace general {

class EXISTS final : public org::minima::kissvm::functions::MinimaFunction {
public:
    EXISTS();

    // Execute function: returns TRUE if variable exists, else FALSE
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Match Java behavior: required minimum parameter number
    bool isRequiredMinimumParameterNumber() const override;

    // At least 1 parameter
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace general
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org