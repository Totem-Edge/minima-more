#pragma once

#include <memory>
#include <string>

// Inherit requires full base class header per rules
#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; class StringValue; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace string {

class REPLACE final : public org::minima::kissvm::functions::MinimaFunction {
public:
    REPLACE();

    // Execute and return a Value
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Factory for a new copy of this function
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;

    // How many parameters are required
    int requiredParams() override;

    // Safely replace strings with length check (mirrors Java)
    static std::string safeReplaceAll(const std::string& zStart,
                                      const std::string& zSearch,
                                      const std::string& zReplace);
};

} // namespace string
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org