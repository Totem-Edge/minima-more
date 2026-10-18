#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
namespace values { class Value; }
namespace functions {

class SUBSTR final : public MinimaFunction {
public:
    SUBSTR();

    // Execute function: returns a new Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Factory for a new instance of this function
    std::unique_ptr<MinimaFunction> getNewFunction() override;

    // Number of required parameters
    int requiredParams() override;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org