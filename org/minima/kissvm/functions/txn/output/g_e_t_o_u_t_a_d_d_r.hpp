#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

// Forward declarations to minimize header dependencies (Pitfall 4)
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {

class GETOUTADDR final : public MinimaFunction {
public:
    GETOUTADDR();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    std::unique_ptr<MinimaFunction> getNewFunction() override;
    int requiredParams() override;
};

} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org