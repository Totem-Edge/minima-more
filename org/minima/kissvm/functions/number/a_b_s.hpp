#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace number {

class ABS : public MinimaFunction {
public:
    ABS();
    ~ABS() override = default;

    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    int requiredParams() override;

    std::unique_ptr<MinimaFunction> getNewFunction() override;
};

} // namespace number
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org