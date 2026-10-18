#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {
class Contract;
namespace values { class Value; }
}
}
}

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace txn {
namespace output {

class GETOUTTOK final : public org::minima::kissvm::functions::MinimaFunction {
public:
    GETOUTTOK();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;

    int requiredParams() override;
};

} // namespace output
} // namespace txn
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org