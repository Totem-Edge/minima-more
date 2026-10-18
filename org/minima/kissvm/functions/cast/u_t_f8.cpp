#include "org/minima/kissvm/functions/cast/u_t_f8.hpp"

#include <vector>
#include <cstdint>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace functions {
namespace cast {

UTF8::UTF8() : MinimaFunction("UTF8") {}

std::unique_ptr<org::minima::kissvm::values::Value>
UTF8::runFunction(org::minima::kissvm::Contract& zContract) {
    // Ensure correct number of parameters
    checkExactParamNumber(requiredParams());

    // Get the HEX parameter
    std::unique_ptr<org::minima::kissvm::values::HexValue> hex = zContract.getHexParam(0, *this);

    // Convert raw bytes to a UTF-8 string
    std::vector<std::uint8_t> bytes = hex->getRawData();
    std::string newstr;
    if (!bytes.empty()) {
        newstr.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    } else {
        newstr.clear();
    }

    // Return as a StringValue
    return std::make_unique<org::minima::kissvm::values::StringValue>(newstr);
}

int UTF8::requiredParams() {
    return 1;
}

std::unique_ptr<MinimaFunction> UTF8::getNewFunction() {
    return std::make_unique<UTF8>();
}

} // namespace cast
} // namespace functions
} // namespace kissvm
} // namespace minima
} // namespace org