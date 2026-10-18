#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
} // namespace kissvm
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace sha {

class PROOF final : public org::minima::kissvm::functions::MinimaFunction {
public:
    PROOF();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
    int requiredParams() override;
};

} // namespace sha
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org