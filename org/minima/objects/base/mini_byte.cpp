#include "org/minima/objects/base/mini_byte.hpp"

#include <stdexcept>
#include <ios>

namespace org {
namespace minima {
namespace objects {
namespace base {

// Define static constants
const MiniByte& MiniByte::FALSE() {
    static const MiniByte instance(0);
    return instance;
}
const MiniByte& MiniByte::TRUE() {
    static const MiniByte instance(1);
    return instance;
}

// Constructors
MiniByte::MiniByte() : mVal(0) {}

MiniByte::MiniByte(int val)
    : mVal(static_cast<int8_t>(val)) {}

MiniByte::MiniByte(int8_t zVal)
    : mVal(zVal) {}

MiniByte::MiniByte(bool zVal)
    : mVal(zVal ? static_cast<int8_t>(1) : static_cast<int8_t>(0)) {}

// Accessors
int MiniByte::getValue() const {
    return static_cast<int>(static_cast<uint8_t>(mVal));
}

int8_t MiniByte::getByteValue() const {
    return mVal;
}

// Comparisons
bool MiniByte::isEqual(const MiniByte& zRamByte) const {
    return getValue() == zRamByte.getValue();
}

bool MiniByte::isFalse() const {
    return isEqual(MiniByte::FALSE());
}

bool MiniByte::isTrue() const {
    return !isFalse();
}

// String representation
std::string MiniByte::toString() const {
    return std::to_string(getValue());
}

// Streamable
void MiniByte::writeDataStream(std::ostream& out) {
    // Write exactly one byte, matching Java's DataOutputStream.writeByte semantics
    unsigned char byteval = static_cast<unsigned char>(mVal);
    out.put(static_cast<char>(byteval));
    if (!out.good()) {
        throw std::ios_base::failure("MiniByte::writeDataStream failed");
    }
}

void MiniByte::readDataStream(std::istream& in) {
    char c = 0;
    if (!in.get(c)) {
        throw std::ios_base::failure("MiniByte::readDataStream failed");
    }
    // Preserve exact 8-bit pattern; Java readByte returns signed byte
    mVal = static_cast<int8_t>(static_cast<unsigned char>(c));
}

// Static helpers
MiniByte MiniByte::ReadFromStream(std::istream& in) {
    MiniByte data;
    data.readDataStream(in);
    return data;
}

void MiniByte::WriteToStream(std::ostream& out, bool zData) {
    MiniByte(zData).writeDataStream(out);
}

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org