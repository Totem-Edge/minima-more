#pragma once

#include <memory>
#include <string>

// Inherit from MinimaFunction (must include full header for inheritance)
#include "org/minima/kissvm/functions/minima_function.hpp"

// Forward declarations to avoid heavy includes in header
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace string {

class REPLACEFIRST final : public org::minima::kissvm::functions::MinimaFunction {
public:
    REPLACEFIRST();

    // Execute function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Required parameters
    int requiredParams() override;

    // Factory method
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace string
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org