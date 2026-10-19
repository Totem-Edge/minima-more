#include "org/minima/kissvm/values/number_value.hpp"

#include <stdexcept>

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

namespace {
// Java's MiniNumber is bounded to +/- (2^64-1); our internal MiniNumber is wider
// (for chain weights/difficulty). Enforce the Java bound at the KISSVM value
// boundary so scripts are accepted/rejected identically by both implementations.
const MiniNumber& javaMax() {
    static const MiniNumber m(std::string("18446744073709551615"));  // 2^64 - 1
    return m;
}
const MiniNumber& javaMin() {
    static const MiniNumber m = MiniNumber(std::string("18446744073709551615")).mult(MiniNumber::MINUSONE());
    return m;
}
void checkJavaRange(const MiniNumber& n) {
    if (n.isMore(javaMax()) || n.isLess(javaMin())) {
        throw std::runtime_error("NumberValue: magnitude exceeds 2^64-1 (Java parity)");
    }
}
}  // namespace

NumberValue::NumberValue(int zValue)
    : mNumber(MiniNumber(std::to_string(zValue))) {}

NumberValue::NumberValue(long long zValue)
    : mNumber(MiniNumber(std::to_string(zValue))) {}

NumberValue::NumberValue(const MiniNumber& zValue)
    : mNumber(zValue) { checkJavaRange(mNumber); }

NumberValue::NumberValue(const std::string& zNumber)
    : mNumber(MiniNumber(zNumber)) { checkJavaRange(mNumber); }

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