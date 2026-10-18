#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

class MIN : public org::minima::kissvm::functions::MinimaFunction {
public:
    MIN();

    // Overrides
    std::unique_ptr<org::minima::kissvm::values::Value> runFunction(org::minima::kissvm::Contract& zContract) override;
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
    int requiredParams() override;
    bool isRequiredMinimumParameterNumber() const override;
};

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org