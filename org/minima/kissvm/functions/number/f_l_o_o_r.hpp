#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

class FLOOR final : public org::minima::kissvm::functions::MinimaFunction {
public:
    FLOOR();

    // Execute function: FLOOR(number) -> NumberValue(floor(number))
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Required parameters count
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org