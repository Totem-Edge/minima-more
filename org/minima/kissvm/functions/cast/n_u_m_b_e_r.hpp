#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

class NUMBER final : public MinimaFunction {
public:
    NUMBER();

    // Execute the function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Factory method
    std::unique_ptr<MinimaFunction> getNewFunction() override;

    // Exactly one parameter required
    int requiredParams() override;
};

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org