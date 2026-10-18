#include "org/minima/utils/base_converter.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

#include "org/minima/objects/base/mini_data.hpp" // used in optional test main

namespace org {
namespace minima {
namespace utils {

namespace {

// Uppercase hex digit lookup
constexpr char HEX16ARRAY[] = "0123456789ABCDEF";

inline bool is_hex_upper(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

inline int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

inline std::string to_upper_ascii(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

inline std::string to_lower_ascii(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// Trim leading zero bytes from big-endian magnitude
inline std::vector<std::uint8_t> trim_leading_zeros(const std::vector<std::uint8_t>& be) {
    std::size_t i = 0;
    while (i < be.size() && be[i] == 0) ++i;
    if (i == be.size()) return {}; // represent zero as empty magnitude
    return std::vector<std::uint8_t>(be.begin() + static_cast<std::ptrdiff_t>(i), be.end());
}

// Divide big-endian magnitude by small base (>=2), returning remainder.
// Modifies value in-place to quotient (trimmed). Returns remainder.
inline std::uint32_t divmod_small(std::vector<std::uint8_t>& mag, std::uint32_t base) {
    std::uint32_t rem = 0;
    for (std::size_t i = 0; i < mag.size(); ++i) {
        std::uint32_t cur = (rem << 8) | mag[i];
        mag[i] = static_cast<std::uint8_t>(cur / base);
        rem = cur % base;
    }
    // trim leading zeros
    std::size_t j = 0;
    while (j < mag.size() && mag[j] == 0) ++j;
    if (j > 0) {
        mag.erase(mag.begin(), mag.begin() + static_cast<std::ptrdiff_t>(j));
    }
    return rem;
}

// Multiply big-endian magnitude by small base and add small addend.
inline void muladd_small(std::vector<std::uint8_t>& mag, std::uint32_t base, std::uint32_t addend) {
    // Multiply by base
    std::uint32_t carry = 0;
    for (std::size_t i = mag.size(); i-- > 0;) {
        std::uint32_t prod = static_cast<std::uint32_t>(mag[i]) * base + carry;
        mag[i] = static_cast<std::uint8_t>(prod & 0xFF);
        carry = prod >> 8;
    }
    while (carry > 0) {
        mag.insert(mag.begin(), static_cast<std::uint8_t>(carry & 0xFF));
        carry >>= 8;
    }
    // Add addend
    carry = addend;
    for (std::size_t i = mag.size(); i-- > 0 && carry > 0;) {
        std::uint32_t sum = static_cast<std::uint32_t>(mag[i]) + carry;
        mag[i] = static_cast<std::uint8_t>(sum & 0xFF);
        carry = sum >> 8;
    }
    while (carry > 0) {
        mag.insert(mag.begin(), static_cast<std::uint8_t>(carry & 0xFF));
        carry >>= 8;
    }
}

// Base32 digit maps (lowercase for encode/decode core)
inline char base32_digit_from_val(std::uint32_t v) {
    // 0-9 => '0'..'9'; 10-31 => 'a'..'v'
    return (v < 10) ? static_cast<char>('0' + v)
                    : static_cast<char>('a' + (v - 10));
}

inline int base32_val_from_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'v') return 10 + (c - 'a');
    return -1;
}

} // anonymous namespace

// BASE 16
std::string BaseConverter::encode16(const std::vector<std::uint8_t>& bytes) {
    return encode16(bytes.data(), bytes.size());
}

std::string BaseConverter::encode16(const std::uint8_t* data, std::size_t len) {
    if (len == 0 || data == nullptr) {
        return std::string();
    }
    std::string out;
    out.reserve(2 + len * 2);
    out.append("0x");
    for (std::size_t i = 0; i < len; ++i) {
        const std::uint8_t b = data[i];
        out.push_back(HEX16ARRAY[(b >> 4) & 0x0F]);
        out.push_back(HEX16ARRAY[b & 0x0F]);
    }
    return out;
}

std::vector<std::uint8_t> BaseConverter::decode16(const std::string& zHex) {
    std::string hex = zHex;
    // Strip optional 0x/0X prefix
    std::string lower = to_lower_ascii(hex);
    if (lower.rfind("0x", 0) == 0) {
        if (hex.size() < 2) {
            throw std::invalid_argument("Invalid HEX string in decode16: " + zHex);
        }
        hex = hex.substr(2);
    }

    // Uppercase and validate
    hex = to_upper_ascii(hex);
    const std::size_t len = hex.size();

    if (len == 0) {
        return {};
    }

    for (char c : hex) {
        if (!is_hex_upper(c)) {
            throw std::invalid_argument("Invalid HEX string in decode16 : " + zHex);
        }
    }

    std::string hex_adj = hex;
    if (len % 2 != 0) {
        hex_adj = "0" + hex;
    }

    std::vector<std::uint8_t> data;
    data.resize(hex_adj.size() / 2);
    for (std::size_t i = 0; i < hex_adj.size(); i += 2) {
        int hi = hex_nibble(hex_adj[i]);
        int lo = hex_nibble(hex_adj[i + 1]);
        data[i / 2] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return data;
}

std::string BaseConverter::numberToHex(int zNumber) {
    // Replicate Java Integer.toHexString (two's complement of 32-bit int, no leading zeros)
    std::uint32_t v = static_cast<std::uint32_t>(zNumber);
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << v; // lowercase hex, no leading zeros
    std::string hex = oss.str();
    if (hex.size() % 2 != 0) {
        hex.insert(hex.begin(), '0');
    }
    // Uppercase and prefix
    for (auto& ch : hex) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return std::string("0x") + hex;
}

int BaseConverter::hexToNumber(const std::string& zHex) {
    if (zHex.size() < 2) {
        throw std::invalid_argument("hexToNumber requires string of length >= 2 with 0x prefix");
    }
    // Java: new BigInteger(zHex.substring(2), 16).intValue();
    std::string hex = zHex.substr(2);
    // Allow both cases
    // Accumulate low 32 bits only, matching BigInteger::intValue truncation behavior
    std::uint32_t acc = 0;
    for (char c : hex) {
        int nib = hex_nibble(c);
        if (nib < 0) {
            throw std::invalid_argument("Invalid hex digit in hexToNumber: " + zHex);
        }
        acc = (acc << 4) | static_cast<std::uint32_t>(nib);
    }
    // Interpret as signed 32-bit int
    return static_cast<int32_t>(acc);
}

std::string BaseConverter::encode32(const std::vector<std::uint8_t>& zData) {
    // BigInteger(1, zData) semantics: positive magnitude, leading zeros trimmed
    std::vector<std::uint8_t> mag = trim_leading_zeros(zData);

    std::string digits;
    if (mag.empty()) {
        digits = "0";
    } else {
        // Repeated divmod by 32 collecting remainders
        std::vector<std::uint8_t> tmp = mag;
        while (!tmp.empty()) {
            std::uint32_t rem = divmod_small(tmp, 32);
            digits.push_back(base32_digit_from_val(rem));
        }
        std::reverse(digits.begin(), digits.end());
    }

    // Replace problematic characters on lowercase string
    for (auto& ch : digits) {
        if (ch == 'i') ch = 'w';
        else if (ch == 'l') ch = 'y';
        else if (ch == 'o') ch = 'z';
    }

    // Uppercase and prefix "Mx"
    for (auto& ch : digits) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return std::string("Mx") + digits;
}

std::vector<std::uint8_t> BaseConverter::decode32(const std::string& zBase32) {
    // toLowerCase
    std::string b32 = to_lower_ascii(zBase32);
    // Strip "mx" prefix if present
    if (b32.rfind("mx", 0) == 0) {
        b32 = b32.substr(2);
    }

    // Reverse replacements
    for (auto& ch : b32) {
        if (ch == 'w') ch = 'i';
        else if (ch == 'y') ch = 'l';
        else if (ch == 'z') ch = 'o';
    }

    if (b32.empty()) {
        // Java BigInteger("", 32) would throw NumberFormatException
        throw std::invalid_argument("Invalid Base32 string in decode32: empty payload");
    }

    // Validate and convert base32 -> big integer (byte magnitude)
    std::vector<std::uint8_t> mag; // big-endian
    for (char ch : b32) {
        int val = base32_val_from_digit(ch);
        if (val < 0) {
            throw std::invalid_argument("Invalid Base32 character in decode32: " + std::string(1, ch));
        }
        muladd_small(mag, 32, static_cast<std::uint32_t>(val));
    }

    // mag is the big-endian byte vector of the integer value. This matches:
    // MiniData hexval = new MiniData("0x"+bigint.toString(16)); return hexval.getBytes();
    // which returns trimmed magnitude bytes.
    // Ensure no leading zeros (muladd_small never produces them).
    return mag;
}

} // namespace utils
} // namespace minima
} // namespace org

// Optional test main replicating the Java example.
// Enable with -DBASE_CONVERTER_BUILD_MAIN to build this file as a standalone test.
#ifdef BASE_CONVERTER_BUILD_MAIN
#include <iostream>
int main(int argc, char* argv[]) {
    using org::minima::objects::base::MiniData;
    using org::minima::utils::BaseConverter;

    MiniData hex("0x000001");
    std::string hstr = hex.to0xString();
    std::cout << "HEX : " << hstr.length() << " " << hstr << std::endl;

    std::string base32 = BaseConverter::encode32(hex.getBytes());
    std::cout << "B32 : " << base32.length() << " " << base32 << std::endl;

    std::vector<std::uint8_t> convbytes = BaseConverter::decode32(base32);
    MiniData conv(convbytes);
    std::string convstr = conv.to0xString();
    std::cout << "COV : " << convstr.length() << " " << convstr << std::endl;

    std::cout << (conv.isEqual(hex) ? "true" : "false") << std::endl;
    return 0;
}
#endif