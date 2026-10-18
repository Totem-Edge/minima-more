#include "org/minima/kissvm/values/boolean_value.hpp"

// Include full Value definition to access VALUE_BOOLEAN and ensure proper linkage
#include "org/minima/kissvm/values/value.hpp"

// Add the namespace wrapper
namespace org {
namespace minima {
namespace kissvm {
namespace values {

BooleanValue::BooleanValue(bool zValue)
    : mTrue(zValue) {}

BooleanValue::~BooleanValue() = default;

bool BooleanValue::isTrue() const {
    return mTrue;
}

bool BooleanValue::isFalse() const {
    return !mTrue;
}

std::string BooleanValue::toString() const {
    return mTrue ? "TRUE" : "FALSE";
}

int BooleanValue::getValueType() const {
    return Value::VALUE_BOOLEAN;
}

bool BooleanValue::isEqual(const BooleanValue& zBool) const {
    return mTrue == zBool.isTrue();
}

BooleanValue* BooleanValue::clone() const {
    return new BooleanValue(mTrue);  
}


// Close the namespaces
} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org