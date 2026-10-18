#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

class LEN final : public org::minima::kissvm::functions::MinimaFunction {
public:
    LEN();

    // MinimaFunction overrides
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    int requiredParams() override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org