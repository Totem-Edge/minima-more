#pragma once

#include "org/minima/kissvm/functions/minima_function.hpp"

#include <memory>

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; class NumberValue; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

class POW final : public org::minima::kissvm::functions::MinimaFunction {
public:
    POW();

    // MinimaFunction overrides
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    int requiredParams() override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org