#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
namespace values { class Value; }
namespace functions {
namespace cast {

class BOOL final : public org::minima::kissvm::functions::MinimaFunction {
public:
    BOOL();

    // Execute the function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Parameter requirements
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org