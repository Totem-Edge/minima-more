#pragma once

#include <memory>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

class SHA3 : public MinimaFunction {
public:
    SHA3();

    // Execute: run and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Factory: return a new copy of this function
    std::unique_ptr<MinimaFunction> getNewFunction() override;

    // Parameter expectations
    int requiredParams() override;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org