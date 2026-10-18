#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

// Forward declarations to avoid heavy includes in header (PITFALL 4)
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace input {

class SUMINPUTS final : public org::minima::kissvm::functions::MinimaFunction {
public:
    SUMINPUTS();

    // MinimaFunction overrides
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction>
    getNewFunction() override;

    int requiredParams() override;
};

} // namespace input
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org