#pragma once

#include <memory>
#include <vector>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

class BITCOUNT final : public org::minima::kissvm::functions::MinimaFunction {
public:
    BITCOUNT();

    // Execute function
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    // Required parameters
    int requiredParams() override;

    // Factory
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;

    // Static utility equivalent to Java's totalBits(byte[])
    static int totalBits(const std::vector<unsigned char>& zData);

private:
    // Lookup table for bits per byte (256 entries)
    static const int BITSPERBYTE[256];
};

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org