#pragma once

#include <memory>
#include <string>
#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace state {

class SAMESTATE final : public org::minima::kissvm::functions::MinimaFunction {
public:
    SAMESTATE();

    // Virtual destructor
    ~SAMESTATE() override = default;

    // Execute function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Required parameter count
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace state
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org