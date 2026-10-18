#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

class VERIFYIN final : public org::minima::kissvm::functions::MinimaFunction {
public:
    VERIFYIN();
    ~VERIFYIN() override = default;

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value> runFunction(org::minima::kissvm::Contract& zContract) override;
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
    int requiredParams() override;
};

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org