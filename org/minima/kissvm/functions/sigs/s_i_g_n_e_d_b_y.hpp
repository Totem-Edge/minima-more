#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {

class Contract;

namespace values {
class Value;
class HexValue;
class BooleanValue;
}

namespace functions {
namespace sigs {

class SIGNEDBY final : public org::minima::kissvm::functions::MinimaFunction {
public:
    SIGNEDBY();

    // Execute function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Required parameter count
    int requiredParams() override;

    // Factory for a new instance
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;

    // Virtual destructor
    ~SIGNEDBY() override = default;
};

} // namespace sigs
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org