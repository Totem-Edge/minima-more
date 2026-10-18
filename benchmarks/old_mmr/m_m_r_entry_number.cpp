#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace org {
namespace minima {
namespace objects {
namespace mmr {

using boost::multiprecision::cpp_int;

// Static constants
const MMREntryNumber MMREntryNumber::ZERO = MMREntryNumber(0);
const MMREntryNumber MMREntryNumber::TWO  = MMREntryNumber(2);

// Constructors
MMREntryNumber::MMREntryNumber() : m_unscaled(0), m_scale(0) {}

MMREntryNumber::MMREntryNumber(int zNumber) : m_unscaled(zNumber), m_scale(0) {}

MMREntryNumber::MMREntryNumber(const cpp_int& zBigInteger) : m_unscaled(zBigInteger), m_scale(0) {}

// Accessor analogous to Java getBigDecimal(): plain decimal string
std::string MMREntryNumber::getBigDecimal() const {
    return toString();
}

// Helpers
cpp_int MMREntryNumber::pow10(unsigned int n) {
    static const cpp_int TEN = cpp_int(10);
    cpp_int result = 1;
    for (unsigned int i = 0; i < n; ++i) {
        result *= TEN;
    }
    return result;
}

std::vector<std::uint8_t> MMREntryNumber::to_twos_complement_bytes(const cpp_int& value) {
    std::vector<std::uint8_t> out;

    if (value == 0) {
        out.push_back(0);
        return out;
    }

    if (value > 0) {
        // Export magnitude
        cpp_int tmp = value;
        while (tmp > 0) {
            std::uint8_t b = static_cast<std::uint8_t>(tmp & 0xFF);
            out.push_back(b);
            tmp >>= 8;
        }
        std::reverse(out.begin(), out.end());
        // Ensure sign bit is 0; add leading 0x00 if MSB has top bit set.
        if (out[0] & 0x80) {
            out.insert(out.begin(), 0x00);
        }
        return out;
    }

    // Negative: find minimal number of bytes n such that value fits in n-byte two's complement
    // Condition: -2^(8n-1) <= value < 2^(8n-1)
    int n = 1;
    while (true) {
        cpp_int lower = -(cpp_int(1) << (8 * n - 1));
        cpp_int upper =  (cpp_int(1) << (8 * n - 1));
        if (value >= lower && value < upper) {
            break;
        }
        ++n;
    }

    // Compute two's complement representation as value mod 2^(8n)
    cpp_int mod = (cpp_int(1) << (8 * n));
    cpp_int twos = mod + value; // since value is negative, this yields 0..mod-1

    // Export exactly n bytes big-endian
    out.resize(n, 0);
    for (int i = n - 1; i >= 0; --i) {
        out[i] = static_cast<std::uint8_t>(twos & 0xFF);
        twos >>= 8;
    }
    return out;
}

cpp_int MMREntryNumber::from_twos_complement_bytes(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) {
        return cpp_int(0);
    }

    // Interpret big-endian two's complement
    const bool negative = (bytes[0] & 0x80) != 0;
    cpp_int val = 0;
    for (std::uint8_t b : bytes) {
        val <<= 8;
        val += cpp_int(b);
    }

    if (!negative) {
        return val;
    }

    // Negative: subtract 2^(8n)
    int n = static_cast<int>(bytes.size());
    cpp_int mod = (cpp_int(1) << (8 * n));
    return val - mod;
}

int MMREntryNumber::compare_values(const cpp_int& ua, int sa,
                                   const cpp_int& ub, int sb) {
    if (ua == 0 && ub == 0) {
        return 0;
    }
    int s = std::max(sa, sb);
    cpp_int a = ua * pow10(static_cast<unsigned int>(s - sa));
    cpp_int b = ub * pow10(static_cast<unsigned int>(s - sb));
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

void MMREntryNumber::align_to_scale(const cpp_int& ua, int sa,
                                    const cpp_int& ub, int sb,
                                    int targetScale,
                                    cpp_int& outA,
                                    cpp_int& outB) {
    // Assumes targetScale >= sa and >= sb
    outA = ua * pow10(static_cast<unsigned int>(targetScale - sa));
    outB = ub * pow10(static_cast<unsigned int>(targetScale - sb));
}

// toString helper: plain string without exponent, strip trailing zeros after decimal point
std::string MMREntryNumber::to_plain_string_strip_trailing_zeros(const cpp_int& unscaled, int scale) {
    if (unscaled == 0) {
        return std::string("0");
    }

    bool neg = unscaled < 0;
    cpp_int absu = neg ? -unscaled : unscaled;

    // Convert abs(unscaled) to decimal string
    std::string digits = absu.convert_to<std::string>();

    if (scale <= 0) {
        // No decimal point; effectively append zeros if scale < 0
        if (neg) {
            return "-" + digits + std::string(static_cast<std::size_t>(-scale), '0');
        } else {
            return digits + std::string(static_cast<std::size_t>(-scale), '0');
        }
    }

    // Insert decimal point scale digits from right
    if (static_cast<int>(digits.size()) <= scale) {
        // Need leading zeros
        std::string out = "0.";
        out.append(static_cast<std::size_t>(scale - digits.size()), '0');
        out += digits;

        // Strip trailing zeros after decimal
        while (!out.empty() && out.back() == '0') out.pop_back();
        if (!out.empty() && out.back() == '.') out.pop_back();

        if (neg && out != "0") out.insert(out.begin(), '-');
        return out;
    } else {
        // Place the decimal point inside the string
        std::size_t ipos = digits.size() - static_cast<std::size_t>(scale);
        std::string out = digits.substr(0, ipos);
        out.push_back('.');
        out += digits.substr(ipos);

        // Strip trailing zeros after decimal
        while (!out.empty() && out.back() == '0') out.pop_back();
        if (!out.empty() && out.back() == '.') out.pop_back();

        if (neg && out != "0") out.insert(out.begin(), '-');
        return out;
    }
}

// Operations
MMREntryNumber MMREntryNumber::modulo(const MMREntryNumber& zNumber) const {
    if (zNumber.m_unscaled == 0) {
        throw std::runtime_error("Division by zero in MMREntryNumber::modulo");
    }

    // Compute q = trunc( (ua * 10^(sb - sa)) / ub )
    // Then r = a - q*b, with scale = max(sa, sb)
    const cpp_int& ua = m_unscaled;
    int sa = m_scale;
    const cpp_int& ub = zNumber.m_unscaled;
    int sb = zNumber.m_scale;

    cpp_int num = ua;
    cpp_int den = ub;

    int exp = sb - sa;
    if (exp >= 0) {
        num *= pow10(static_cast<unsigned int>(exp));
    } else {
        den *= pow10(static_cast<unsigned int>(-exp));
    }

    // Truncate toward zero
    cpp_int q = num / den;

    // r = a - q*b at scale s = max(sa, sb)
    int s = std::max(sa, sb);
    cpp_int a_scaled, b_scaled;
    align_to_scale(ua, sa, ub, sb, s, a_scaled, b_scaled);
    cpp_int r_unscaled = a_scaled - q * b_scaled;

    MMREntryNumber res;
    res.m_unscaled = r_unscaled;
    res.m_scale = s;
    return res;
}

MMREntryNumber MMREntryNumber::floor() const {
    // Return integer floor with scale 0
    MMREntryNumber res;
    if (m_scale <= 0) {
        // Already integer times 10^k, k>=0
        res.m_unscaled = m_unscaled * pow10(static_cast<unsigned int>(-m_scale));
        res.m_scale = 0;
        return res;
    }

    cpp_int denom = pow10(static_cast<unsigned int>(m_scale));
    cpp_int q = m_unscaled / denom; // trunc toward zero
    cpp_int r = m_unscaled % denom;

    if (m_unscaled >= 0 || r == 0) {
        res.m_unscaled = q;
    } else {
        // Negative with fractional part -> floor is q - 1
        res.m_unscaled = q - 1;
    }
    res.m_scale = 0;
    return res;
}

MMREntryNumber MMREntryNumber::increment() const {
    // Equivalent to add(BigDecimal.ONE) with result scale = max(m_scale, 0)
    int s = std::max(m_scale, 0);
    cpp_int a_scaled = m_unscaled * pow10(static_cast<unsigned int>(s - m_scale));
    cpp_int one_scaled = cpp_int(1) * pow10(static_cast<unsigned int>(s));

    MMREntryNumber res;
    res.m_unscaled = a_scaled + one_scaled;
    res.m_scale = s;
    return res;
}

MMREntryNumber MMREntryNumber::decrement() const {
    int s = std::max(m_scale, 0);
    cpp_int a_scaled = m_unscaled * pow10(static_cast<unsigned int>(s - m_scale));
    cpp_int one_scaled = cpp_int(1) * pow10(static_cast<unsigned int>(s));

    MMREntryNumber res;
    res.m_unscaled = a_scaled - one_scaled;
    res.m_scale = s;
    return res;
}

MMREntryNumber MMREntryNumber::div2() const {
    MMREntryNumber res = *this;
    if ((res.m_unscaled & 1) == 0) {
        // Even -> divide unscaled by 2
        res.m_unscaled >>= 1;
    } else {
        // Odd -> multiply by 5 and increase scale by 1 (exact decimal)
        res.m_unscaled *= 5;
        res.m_scale += 1;
    }
    return res;
}

MMREntryNumber MMREntryNumber::mult2() const {
    MMREntryNumber res = *this;
    res.m_unscaled <<= 1;
    return res;
}

int MMREntryNumber::compareTo(const MMREntryNumber& zCompare) const {
    return compare_values(m_unscaled, m_scale, zCompare.m_unscaled, zCompare.m_scale);
}

bool MMREntryNumber::isEqual(const MMREntryNumber& zNumber) const {
    return compareTo(zNumber) == 0;
}

bool MMREntryNumber::isLess(const MMREntryNumber& zNumber) const {
    return compareTo(zNumber) < 0;
}

bool MMREntryNumber::isLessEqual(const MMREntryNumber& zNumber) const {
    return compareTo(zNumber) <= 0;
}

bool MMREntryNumber::isMore(const MMREntryNumber& zNumber) const {
    return compareTo(zNumber) > 0;
}

bool MMREntryNumber::isMoreEqual(const MMREntryNumber& zNumber) const {
    return compareTo(zNumber) >= 0;
}

std::string MMREntryNumber::toString() const {
    return to_plain_string_strip_trailing_zeros(m_unscaled, m_scale);
}

// Streamable
void MMREntryNumber::writeDataStream(std::ostream& out) {
    // Write scale
    org::minima::objects::base::MiniNumber::WriteToStream(out, m_scale);

    // Write unscaled value as Java BigInteger two's-complement bytes
    std::vector<std::uint8_t> bytes = to_twos_complement_bytes(m_unscaled);
    org::minima::objects::base::MiniData::WriteToStream(out, bytes);
}

void MMREntryNumber::readDataStream(std::istream& in) {
    // Scale
    org::minima::objects::base::MiniNumber scaleNum = org::minima::objects::base::MiniNumber::ReadFromStream(in);
    int scale = scaleNum.getAsInt();

    // Unscaled bytes
    org::minima::objects::base::MiniData unscaleddata = org::minima::objects::base::MiniData::ReadFromStream(in);
    const std::vector<std::uint8_t>& bytes = unscaleddata.getBytes();

    // Construct number
    m_unscaled = from_twos_complement_bytes(bytes);
    m_scale = scale;
}

// Static helper
MMREntryNumber MMREntryNumber::ReadFromStream(std::istream& in) {
    MMREntryNumber data;
    data.readDataStream(in);
    return data;
}

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org