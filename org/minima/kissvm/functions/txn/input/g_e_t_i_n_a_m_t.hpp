#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

class GETINAMT final : public org::minima::kissvm::functions::MinimaFunction {
public:
    GETINAMT();

    // Execute function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Parameters required
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org