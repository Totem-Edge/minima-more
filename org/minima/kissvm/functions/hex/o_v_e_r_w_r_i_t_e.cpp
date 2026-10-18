#include "org/minima/kissvm/functions/hex/o_v_e_r_w_r_i_t_e.hpp"

#include <algorithm>
#include <vector>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace hex {

OVERWRITE::OVERWRITE()
    : org::minima::kissvm::functions::MinimaFunction("OVERWRITE") {}

std::unique_ptr<org::minima::kissvm::values::Value>
OVERWRITE::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure correct number of parameters
    checkExactParamNumber(requiredParams());

    // Source MiniData and source start position
    auto srcHex = zContract.getHexParam(0, *this);
    const org::minima::objects::base::MiniData& src = srcHex->getMiniData();
    int srcpos = zContract.getNumberParam(1, *this)->getNumber().getAsInt();

    // Destination MiniData (make a copy of the original's bytes)
    auto destHex = zContract.getHexParam(2, *this);
    const org::minima::objects::base::MiniData& destorig = destHex->getMiniData();
    std::vector<std::uint8_t> destbytes = destorig.getBytes();
    int destpos = zContract.getNumberParam(3, *this)->getNumber().getAsInt();

    // Length to copy
    int len = zContract.getNumberParam(4, *this)->getNumber().getAsInt();

    // Checks
    int destlen = static_cast<int>(destbytes.size());
    if (destpos + len > destlen) {
        throw org::minima::kissvm::exceptions::ExecutionException("OVERWRITE destination array too short");
    } else if (srcpos + len > src.getLength()) {
        throw org::minima::kissvm::exceptions::ExecutionException("OVERWRITE src array too short");
    } else if (len < 0) {
        throw org::minima::kissvm::exceptions::ExecutionException(
            std::string("Cannot have negative length ") + std::to_string(len));
    }

    // Perform the overwrite
    const std::vector<std::uint8_t>& srcbytes = src.getBytes();
    std::copy(srcbytes.begin() + srcpos,
              srcbytes.begin() + srcpos + len,
              destbytes.begin() + destpos);

    // Return new HexValue from modified destination bytes
    return std::make_unique<org::minima::kissvm::values::HexValue>(destbytes);
}

int OVERWRITE::requiredParams() {
    return 5;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> OVERWRITE::getNewFunction() {
    return std::make_unique<OVERWRITE>();
}

} // namespace hex
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org