#include "org/minima/kissvm/values/string_value.hpp"

#include <utility>
#include "org/minima/kissvm/contract.hpp"
#include "org/minima/objects/base/mini_string.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace values {

// Special members definitions
StringValue::~StringValue() = default;
StringValue::StringValue(StringValue&&) noexcept = default;
StringValue& StringValue::operator=(StringValue&&) noexcept = default;

// Constructor
StringValue::StringValue(const std::string& zScript)
    : mScript(std::make_unique<org::minima::objects::base::MiniString>(zScript)) {
    const auto bytes = getBytes();
    int len = static_cast<int>(bytes.size());
    if (len > org::minima::kissvm::Contract::MAX_DATA_SIZE) {
        throw std::invalid_argument(
            "MAX String length reached : " + std::to_string(len) + "/" +
            std::to_string(org::minima::kissvm::Contract::MAX_DATA_SIZE));
    }
}

// Methods
std::string StringValue::toString() const {
    return mScript ? mScript->toString() : std::string();
}

std::vector<uint8_t> StringValue::getBytes() const {
    return mScript ? mScript->getData() : std::vector<uint8_t>{};
}

const org::minima::objects::base::MiniString& StringValue::getMiniString() const {
    // Precondition: mScript is always initialized in constructor
    return *mScript;
}

int StringValue::getValueType() const {
    return VALUE_SCRIPT;
}

bool StringValue::isEqual(const StringValue& zValue) const {
    return toString() == zValue.toString();
}

StringValue StringValue::add(const StringValue& zSCValue) const {
    return StringValue(toString() + zSCValue.toString());
}

StringValue* StringValue::clone() const {
    return new StringValue(toString());
}

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org