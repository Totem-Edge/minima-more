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
namespace general {

class GET final : public org::minima::kissvm::functions::MinimaFunction {
public:
    GET();

    // Execute function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Parameter requirements
    bool isRequiredMinimumParameterNumber() const override;
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace general
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org