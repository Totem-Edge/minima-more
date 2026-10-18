#include "org/minima/kissvm/values/hex_value.hpp"

#include <stdexcept>
#include <algorithm>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace values {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

namespace {

// Convert a non-negative cpp_int to Java BigInteger.toByteArray()-compatible bytes:
// - Big-endian two's-complement
// - For zero: {0x00}
// - For positive values: magnitude bytes; if highest bit is 1, prepend 0x00
static std::vector<std::uint8_t> cpp_int_to_java_positive_twos_bytes(const boost::multiprecision::cpp_int& v) {
    if (v < 0) {
        throw std::invalid_argument("HEXValue Number must be positive");
    }
    if (v == 0) {
        return std::vector<std::uint8_t>{0x00};
    }

    boost::multiprecision::cpp_int tmp = v; // non-negative
    std::vector<std::uint8_t> magnitude;
    while (tmp > 0) {
        std::uint8_t byte = static_cast<std::uint8_t>(tmp & 0xFF);
        magnitude.push_back(byte);
        tmp >>= 8;
    }
    std::reverse(magnitude.begin(), magnitude.end()); // big-endian

    if (!magnitude.empty() && (magnitude[0] & 0x80)) {
        std::vector<std::uint8_t> with_sign;
        with_sign.reserve(magnitude.size() + 1);
        with_sign.push_back(0x00);
        with_sign.insert(with_sign.end(), magnitude.begin(), magnitude.end());
        return with_sign;
    }
    return magnitude;
}

} // anonymous namespace

// Special members
HexValue::~HexValue() = default;
HexValue::HexValue(HexValue&&) noexcept = default;
HexValue& HexValue::operator=(HexValue&&) noexcept = default;

// Constructors

HexValue::HexValue(const std::string& zHex)
    : mData(std::make_unique<MiniData>(zHex)) {
    ensure_max_size_or_throw(mData->getLength());
}

HexValue::HexValue(const MiniData& zData)
    : HexValue(zData.getBytes()) {
    // Delegates to vector<byte> constructor
}

HexValue::HexValue(const std::vector<std::uint8_t>& zData)
    : mData(std::make_unique<MiniData>(zData)) {
    ensure_max_size_or_throw(mData->getLength());
}

HexValue::HexValue(const MiniNumber& zNumber) {
    if (zNumber.isLess(MiniNumber::ZERO())) {
        throw std::invalid_argument("HEXValue Number must be positive");
    }
    if (!zNumber.floor().isEqual(zNumber)) {
        throw std::invalid_argument("HEXValue Number must be a whole number");
    }

    // Build bytes as Java BigInteger.toByteArray() for positive
    boost::multiprecision::cpp_int bi = zNumber.getAsBigInteger();
    std::vector<std::uint8_t> bytes = cpp_int_to_java_positive_twos_bytes(bi);

    mData = std::make_unique<MiniData>(bytes);
    ensure_max_size_or_throw(mData->getLength());
}

// Accessors

const MiniData& HexValue::getMiniData() const {
    return *mData;
}

std::vector<std::uint8_t> HexValue::getRawData() const {
    return mData->getBytes();
}

// Value interface

int HexValue::getValueType() const {
    return VALUE_HEX;
}

// Comparisons

bool HexValue::isEqual(const HexValue& zValue) const {
    return mData->isEqual(zValue.getMiniData());
}

// String

std::string HexValue::toString() const {
    return mData->toString();
}

// Clone

HexValue* HexValue::clone() const {
    return new HexValue(*mData);
}

// Private helper

void HexValue::ensure_max_size_or_throw(int len) {
    if (len > Contract::MAX_DATA_SIZE) {
        throw std::invalid_argument(
            "MAX HEX value size reached : " + std::to_string(len) + "/" + std::to_string(Contract::MAX_DATA_SIZE));
    }
}

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org