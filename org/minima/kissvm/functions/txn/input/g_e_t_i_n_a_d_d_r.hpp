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
namespace txn {
namespace input {

class GETINADDR final : public org::minima::kissvm::functions::MinimaFunction {
public:
    GETINADDR();

    // Execute and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // How many parameters are required
    int requiredParams() override;

    // Factory for a new copy of this function
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org