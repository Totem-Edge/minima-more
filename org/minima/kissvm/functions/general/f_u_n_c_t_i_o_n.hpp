#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
namespace values { class Value; }
} // namespace kissvm
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

class FUNCTION final : public MinimaFunction {
public:
    static const std::string FUNCTION_RETURN;

    FUNCTION();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    bool isRequiredMinimumParameterNumber() const override;
    int requiredParams() override;
    std::unique_ptr<MinimaFunction> getNewFunction() override;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org