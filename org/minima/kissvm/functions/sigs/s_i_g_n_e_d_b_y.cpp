#include "org/minima/kissvm/functions/sigs/s_i_g_n_e_d_b_y.hpp"

#include <memory>
#include <iostream>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace sigs {

SIGNEDBY::SIGNEDBY()
    : org::minima::kissvm::functions::MinimaFunction("SIGNEDBY") {}

std::unique_ptr<org::minima::kissvm::values::Value>
SIGNEDBY::runFunction(org::minima::kissvm::Contract& zContract) {
    // std::cerr << "\n➤ SIGNEDBY::runFunction() CALLED\n";
    // std::cerr << std::flush;
    
    checkExactParamNumber(requiredParams());
    
    auto pubkey = zContract.getHexParam(0, *this);
    // std::cerr << "  Parameter pubkey: " << pubkey->getMiniData().to0xString() << "\n";
    // std::cerr << "  Calling checkSignature()...\n";
    // std::cerr << std::flush;
    
    bool valid = zContract.checkSignature(*pubkey);
    
    // std::cerr << "  checkSignature() returned: " << (valid ? "TRUE" : "FALSE") << "\n";
    // std::cerr << std::flush;
    
    return std::make_unique<org::minima::kissvm::values::BooleanValue>(valid);
}

int SIGNEDBY::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> SIGNEDBY::getNewFunction() {
    return std::make_unique<SIGNEDBY>();
}

} // namespace sigs
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org