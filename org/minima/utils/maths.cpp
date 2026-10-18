#include "org/minima/utils/maths.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/params/global_params.hpp"

namespace org {
namespace minima {
namespace utils {

// Static member definitions
const std::string Maths::BI_TWO = "2";

// Internal helpers for big-endian positive integers (MiniData)
namespace {

std::vector<std::uint8_t> trimLeadingZeros(const std::vector<std::uint8_t>& be) {
    std::size_t i = 0;
    while (i < be.size() && be[i] == 0) {
        ++i;
    }
    if (i == be.size()) {
        return std::vector<std::uint8_t>(); // represent zero as empty
    }
    return std::vector<std::uint8_t>(be.begin() + static_cast<std::ptrdiff_t>(i), be.end());
}

int countLeadingZeros8(std::uint8_t v) {
    // Return number of leading zero bits in an 8-bit value
    // v != 0 assumed by caller when used
    int cnt = 0;
    for (int i = 7; i >= 0; --i) {
        if ((v >> i) & 0x1) {
            break;
        }
        ++cnt;
    }
    return cnt;
}

int bitLengthBE(const std::vector<std::uint8_t>& be_raw) {
    auto be = trimLeadingZeros(be_raw);
    if (be.empty()) {
        return 0;
    }
    std::uint8_t msb = be[0];
    int leading = countLeadingZeros8(msb);
    int bits_in_msb = 8 - leading;
    int total = static_cast<int>((be.size() - 1) * 8 + bits_in_msb);
    return total;
}

// Test if bit j (0 = least significant) is set in big-endian positive integer
bool testBitBE(const std::vector<std::uint8_t>& be_raw, int j) {
    if (j < 0) return false;
    auto be = trimLeadingZeros(be_raw);
    if (be.empty()) return false;
    // Index from LSB side
    std::size_t byte_index_from_lsb = static_cast<std::size_t>(j) / 8;
    if (byte_index_from_lsb >= be.size()) return false;
    std::size_t idx = be.size() - 1 - byte_index_from_lsb;
    int bit_in_byte = j % 8;
    std::uint8_t mask = static_cast<std::uint8_t>(1u << bit_in_byte);
    return (be[idx] & mask) != 0;
}

// Compare unsigned big-endian positive integers
int compareUnsignedBE(const std::vector<std::uint8_t>& a_raw, const std::vector<std::uint8_t>& b_raw) {
    auto a = trimLeadingZeros(a_raw);
    auto b = trimLeadingZeros(b_raw);
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

// Shift-left a positive big-endian integer by nbits (nbits >= 0)
std::vector<std::uint8_t> shiftLeftBE(const std::vector<std::uint8_t>& be_raw, int nbits) {
    if (nbits < 0) throw std::invalid_argument("shiftLeftBE: negative shift");
    auto be = trimLeadingZeros(be_raw);
    if (be.empty()) return be;

    int byte_shift = nbits / 8;
    int bit_shift = nbits % 8;

    std::vector<std::uint8_t> out = be;

    if (bit_shift != 0) {
        std::uint8_t carry = 0;
        // Process from LSB to MSB (right to left)
        for (int i = static_cast<int>(out.size()) - 1; i >= 0; --i) {
            std::uint16_t val = static_cast<std::uint16_t>(out[static_cast<std::size_t>(i)]);
            std::uint16_t shifted = static_cast<std::uint16_t>((val << bit_shift) & 0xFFu);
            std::uint8_t new_carry = static_cast<std::uint8_t>((val >> (8 - bit_shift)) & ((1u << bit_shift) - 1u));
            out[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(shifted | carry);
            carry = new_carry;
        }
        if (carry != 0) {
            out.insert(out.begin(), carry);
        }
    }

    if (byte_shift > 0) {
        out.insert(out.end(), static_cast<std::size_t>(byte_shift), 0x00);
    }

    // Trim any accidental leading zeros
    out = trimLeadingZeros(out);
    return out;
}

} // anonymous namespace

double Maths::log2(double zDD) {
    // Match Java: Math.log10(z) / Math.log10(2)
    return std::log10(zDD) / std::log10(2.0);
}

int Maths::getSuperLevel(const org::minima::objects::base::MiniData& zDifficulty,
                         const org::minima::objects::base::MiniData& zActual) {
    // Extract big-endian byte arrays
    const std::vector<std::uint8_t>& dbe_raw = zDifficulty.getBytes();
    const std::vector<std::uint8_t>& abe_raw = zActual.getBytes();

    // Trim leading zeros for calculations
    std::vector<std::uint8_t> dbe = trimLeadingZeros(dbe_raw);
    std::vector<std::uint8_t> abe = trimLeadingZeros(abe_raw);

    // Division by zero check (Java BigInteger.divide throws)
    if (abe.empty()) {
        throw std::runtime_error("getSuperLevel: division by zero (zActual == 0)");
    }

    // If difficulty is zero then quotient is zero -> level will clamp to 0
    if (dbe.empty()) {
        return 0;
    }

    int n = bitLengthBE(dbe);
    int m = bitLengthBE(abe);

    int shift = n - m;
    if (shift < 0) {
        // quotient is 0
        return 0; // will clamp anyway
    }

    // Compare difficulty with (actual << shift) to determine quotient bit length
    std::vector<std::uint8_t> a_shifted = shiftLeftBE(abe, shift);
    int cmp = compareUnsignedBE(dbe, a_shifted);

    int quotientBitLen = (cmp < 0) ? shift : (shift + 1);

    int ll2 = (quotientBitLen == 0) ? -1 : (quotientBitLen - 1);
    if (ll2 < 0) {
        ll2 = 0;
    }

    // Clamp to GlobalParams.MINIMA_CASCADE_LEVELS - 1
    int maxlevel = org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS - 1;
    if (ll2 > maxlevel) {
        ll2 = maxlevel;
    }

    return ll2;
}

double Maths::log2BI(const org::minima::objects::base::MiniData& val) {
    const std::vector<std::uint8_t>& raw = val.getBytes();
    // Compute bit length
    int n = bitLengthBE(raw);
    // Build 53-bit mantissa
    std::uint64_t mask = 1ULL << 52; // mantissa is 53 bits (including hidden bit)
    std::uint64_t mantissa = 0ULL;
    int j = 0;
    for (int i = 1; i < 54; i++) {
        j = n - i;
        if (j < 0) break;

        if (testBitBE(raw, j)) mantissa |= mask;
        mask >>= 1;
    }
    // Round up if next bit is 1.
    if (j > 0 && testBitBE(raw, j - 1)) mantissa++;

    // If value is zero -> n == 0 -> mantissa == 0 -> f == 0 -> log(0) == -inf
    double f = mantissa / static_cast<double>(1ULL << 52);

    // Magic number converts from base e to base 2
    constexpr double INV_LN2 = 1.44269504088896340735992468100189213742664595415298;
    return (n - 1 + std::log(f) * INV_LN2);
}

int Maths::compareVersions(const std::string& version1, const std::string& version2) {
    // Split by '.'
    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> out;
        std::size_t start = 0;
        while (start <= s.size()) {
            std::size_t pos = s.find('.', start);
            if (pos == std::string::npos) {
                out.emplace_back(s.substr(start));
                break;
            } else {
                out.emplace_back(s.substr(start, pos - start));
                start = pos + 1;
            }
        }
        // Handle empty string case
        if (s.empty()) out.emplace_back(std::string());
        return out;
    };

    std::vector<std::string> levels1 = split(version1);
    std::vector<std::string> levels2 = split(version2);

    std::size_t length = std::max(levels1.size(), levels2.size());
    for (std::size_t i = 0; i < length; ++i) {
        int v1 = (i < levels1.size() && !levels1[i].empty()) ? std::stoi(levels1[i]) : 0;
        int v2 = (i < levels2.size() && !levels2[i].empty()) ? std::stoi(levels2[i]) : 0;
        if (v1 < v2) return -1;
        if (v1 > v2) return 1;
    }
    return 0;
}

} // namespace utils
} // namespace minima
} // namespace org