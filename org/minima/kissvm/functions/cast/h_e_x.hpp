#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

class HEX final : public org::minima::kissvm::functions::MinimaFunction {
public:
    HEX();

    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    int requiredParams() override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org