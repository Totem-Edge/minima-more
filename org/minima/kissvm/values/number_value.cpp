#include "org/minima/kissvm/values/number_value.hpp"

#ifdef _WIN32
// No OS-specific logic needed here; placeholder for future Windows-specific adaptations if required.
#else
// No POSIX-specific logic needed here.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace values {

using org::minima::objects::base::MiniNumber;

NumberValue::NumberValue(int zValue)
    : mNumber(MiniNumber(std::to_string(zValue))) {}

NumberValue::NumberValue(long long zValue)
    : mNumber(MiniNumber(std::to_string(zValue))) {}

NumberValue::NumberValue(const MiniNumber& zValue)
    : mNumber(zValue) {}

NumberValue::NumberValue(const std::string& zNumber)
    : mNumber(MiniNumber(zNumber)) {}

MiniNumber NumberValue::getNumber() const {
    return mNumber;
}

NumberValue* NumberValue::clone() const {
    return new NumberValue(mNumber);  // Uses copy constructor of MiniNumber
}

int NumberValue::getValueType() const {
    return Value::VALUE_NUMBER;
}

bool NumberValue::isFalse() const {
    return mNumber.isEqual(MiniNumber::ZERO());
}

bool NumberValue::isEqual(const NumberValue& zValue) const {
    return mNumber.isEqual(zValue.getNumber());
}

bool NumberValue::isLess(const NumberValue& zValue) const {
    return mNumber.isLess(zValue.getNumber());
}

bool NumberValue::isLessEqual(const NumberValue& zValue) const {
    return mNumber.isLessEqual(zValue.getNumber());
}

bool NumberValue::isMore(const NumberValue& zValue) const {
    return mNumber.isMore(zValue.getNumber());
}

bool NumberValue::isMoreEqual(const NumberValue& zValue) const {
    return mNumber.isMoreEqual(zValue.getNumber());
}

NumberValue NumberValue::add(const NumberValue& zValue) const {
    return NumberValue(mNumber.add(zValue.getNumber()));
}

NumberValue NumberValue::sub(const NumberValue& zValue) const {
    return NumberValue(mNumber.sub(zValue.getNumber()));
}

NumberValue NumberValue::mult(const NumberValue& zValue) const {
    return NumberValue(mNumber.mult(zValue.getNumber()));
}

NumberValue NumberValue::div(const NumberValue& zValue) const {
    return NumberValue(mNumber.div(zValue.getNumber()));
}

std::string NumberValue::toString() const{
    return mNumber.toString();
}

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org