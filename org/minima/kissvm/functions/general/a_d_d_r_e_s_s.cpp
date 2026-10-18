#include "org/minima/kissvm/functions/general/a_d_d_r_e_s_s.hpp"

#include <memory>
#include <string>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace general {

ADDRESS::ADDRESS()
    : org::minima::kissvm::functions::MinimaFunction("ADDRESS") {
}

std::unique_ptr<org::minima::kissvm::values::Value>
ADDRESS::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure correct number of parameters
    checkExactParamNumber(requiredParams());

    // Get the first parameter as a StringValue
    std::unique_ptr<org::minima::kissvm::values::StringValue> str =
        zContract.getStringParam(0, *this);

    // Convert to an Address using the string
    org::minima::objects::Address addr(str->toString());

    // Return the raw address data wrapped in a HexValue
    return std::make_unique<org::minima::kissvm::values::HexValue>(addr.getAddressData());
}

int ADDRESS::requiredParams() {
    return 1;
}

std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> ADDRESS::getNewFunction() {
    return std::make_unique<ADDRESS>();
}

} // namespace general
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org