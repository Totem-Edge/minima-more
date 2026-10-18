#pragma once

#include <memory>
#include <string>

// Include base class for inheritance (Pitfall 4)
#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
namespace values { class Value; }
namespace functions {
namespace txn {
namespace input {

class GETINTOK final : public org::minima::kissvm::functions::MinimaFunction {
public:
    GETINTOK();
    ~GETINTOK() override = default;

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;

    int requiredParams() override;
};

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org