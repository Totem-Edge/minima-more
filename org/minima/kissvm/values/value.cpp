#include "org/minima/kissvm/values/value.hpp"

#include "org/minima/kissvm/tokens/script_tokenizer.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"
#include "org/minima/kissvm/values/hex_value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace values {

std::string Value::toString() const {
    // Default fallback; concrete subclasses provide meaningful implementations
    return getValueTypeString(getValueType());
}

void Value::verifyType(int zType) const {
    if (getValueType() != zType) {
        std::string msg = std::string("Incorrect value type, expected ")
                        + getValueTypeString(zType)
                        + " found "
                        + getValueTypeString(getValueType());
        throw std::invalid_argument(msg);
    }
}

std::unique_ptr<Value> Value::getValue(const std::string& zValue) {
    using org::minima::kissvm::tokens::ScriptTokenizer;

    // Check for [ ... ] script/string form
    if (!zValue.empty() && zValue.front() == '[' && zValue.size() >= 2 && zValue.back() == ']') {
        // remove the square brackets..
        std::string sc = zValue.substr(1, zValue.size() - 2);
        return std::make_unique<StringValue>(sc);

    } else if (zValue.rfind("0x", 0) == 0) {
        return std::make_unique<HexValue>(zValue);

    } else if (zValue == "TRUE") {
        return std::make_unique<BooleanValue>(true);

    } else if (zValue == "FALSE") {
        return std::make_unique<BooleanValue>(false);

    } else if ((!zValue.empty() && zValue[0] == '-') || ScriptTokenizer::isNumeric(zValue)) {
        return std::make_unique<NumberValue>(zValue);

    } else {
        throw std::invalid_argument(std::string("Invalid value : ") + zValue);
    }
}

int Value::getValueType(const std::string& zValue) {
    using org::minima::kissvm::tokens::ScriptTokenizer;

    if (!zValue.empty() && zValue.front() == '[' && zValue.size() >= 2 && zValue.back() == ']') {
        // Then initialise the value
        return VALUE_SCRIPT;

    } else if (zValue.rfind("0x", 0) == 0) {
        return VALUE_HEX;

    } else if (zValue == "TRUE") {
        return VALUE_BOOLEAN;

    } else if (zValue == "FALSE") {
        return VALUE_BOOLEAN;

    } else if ((!zValue.empty() && zValue[0] == '-') ||
               ScriptTokenizer::isNumeric(zValue)) {
        return VALUE_NUMBER;

    } else {
        throw std::invalid_argument(std::string("Invalid value type : ") + zValue);
    }
}

std::string Value::getValueTypeString(int zType) {
    if (zType == VALUE_BOOLEAN) {
        return "BOOLEAN";
    } else if (zType == VALUE_HEX) {
        return "HEX";
    } else if (zType == VALUE_NUMBER) {
        return "NUMBER";
    } else if (zType == VALUE_SCRIPT) {
        return "SCRIPT";
    }
    return "ERROR_UNKNOWN_TYPE";
}

void Value::checkSameType(const Value& zV1, const Value& zV2) {
    if (zV1.getValueType() != zV2.getValueType()) {
        throw std::invalid_argument("Operation requires that both value types MUST be the same");
    }
}

void Value::checkSameType(const Value& zV1, const Value& zV2, int zType) {
    if (zV1.getValueType() != zV2.getValueType()) {
        throw std::invalid_argument("Operation requires that both value types MUST be the same");
    }

    if (zV1.getValueType() != zType) {
        std::string msg = std::string("Operation requires that both value types are of type ")
                        + std::to_string(zType);
        throw std::invalid_argument(msg);
    }
}

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org