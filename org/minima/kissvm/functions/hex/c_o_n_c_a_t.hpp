#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/kissvm/functions/minima_function.hpp"

// Forward declarations for types used in signatures only
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

class CONCAT : public org::minima::kissvm::functions::MinimaFunction {
public:
    CONCAT();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value> runFunction(org::minima::kissvm::Contract& zContract) override;
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
    int requiredParams() override;
    bool isRequiredMinimumParameterNumber() const override;
};

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org