#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace sha {

class SHA2 final : public org::minima::kissvm::functions::MinimaFunction {
public:
    SHA2();

    // Execute function: returns HexValue of SHA2(data) where data is from HEX or SCRIPT parameter
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    int requiredParams() override;
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace sha
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org