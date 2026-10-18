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

class SETLEN final : public MinimaFunction {
public:
    SETLEN();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    std::unique_ptr<MinimaFunction> getNewFunction() override;

    int requiredParams() override;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org