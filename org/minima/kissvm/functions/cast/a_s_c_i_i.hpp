#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

class ASCII final : public org::minima::kissvm::functions::MinimaFunction {
public:
    ASCII();

    // Execute function: ASCII(hex) -> StringValue
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Required params
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org