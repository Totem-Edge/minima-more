#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

class MAX : public org::minima::kissvm::functions::MinimaFunction {
public:
    MAX();
    ~MAX() override = default;

    // Executes the function: returns the maximum of numeric parameters.
    std::unique_ptr<org::minima::kissvm::values::Value> runFunction(org::minima::kissvm::Contract& zContract) override;

    bool isRequiredMinimumParameterNumber() const override;
    int requiredParams() override;
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org