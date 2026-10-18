#pragma once
// File: org/minima/kissvm/functions/sigs\c_h_e_c_k_s_i_g.hpp

#include <memory>
#include <string>

#include "org/minima/kissvm/functions/minima_function.hpp"

namespace org {
namespace minima {
namespace kissvm {

class Contract;

} // namespace kissvm
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace sigs {

class CHECKSIG final : public org::minima::kissvm::functions::MinimaFunction {
public:
    CHECKSIG();

    // MinimaFunction interface
    std::unique_ptr<org::minima::kissvm::values::Value>
    runFunction(org::minima::kissvm::Contract& zContract) override;

    int requiredParams() override;

    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> getNewFunction() override;
};

} // namespace sigs
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org