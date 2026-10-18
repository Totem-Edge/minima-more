#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
namespace values { class Value; }

namespace functions {
namespace hex {

class BITGET final : public MinimaFunction {
public:
    BITGET();

    // Execute and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Factory for a new copy of this function
    std::unique_ptr<MinimaFunction> getNewFunction() override;

    // How many parameters are required
    int requiredParams() override;
};

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org