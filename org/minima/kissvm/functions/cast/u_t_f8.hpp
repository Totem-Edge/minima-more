#pragma once

#include <memory>
#include <string>

// Inherit requires full base header
#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

class UTF8 : public MinimaFunction {
public:
    UTF8();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    int requiredParams() override;

    std::unique_ptr<MinimaFunction> getNewFunction() override;
};

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org