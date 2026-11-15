#pragma once

#include "org/minima/objects/base/mini_number.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <string>

namespace minima::analytics {

// Minima amounts are MiniNumber decimals with up to 44 places. The canonical
// storage unit ("raw") is 10^-44, so raw = value * 10^44 and is always an
// integer. MiniNumber::getAsLong() returns only the integer part of the
// decimal, which turns 0.8 MIN into 0 and every sub-1 token balance into 0.
// Anything that stores or compares amounts must go through these helpers.
inline constexpr int kAmountDecimals = 44;

inline boost::multiprecision::cpp_int amountAtoms(
        const org::minima::objects::base::MiniNumber& value) {
    boost::multiprecision::cpp_int atoms = value.getUnscaled();
    const int scale = value.getScale();
    for (int i = scale; i < kAmountDecimals; ++i) atoms *= 10;
    for (int i = kAmountDecimals; i < scale; ++i) atoms /= 10;
    return atoms;
}

inline std::string amountAtomsString(
        const org::minima::objects::base::MiniNumber& value) {
    return amountAtoms(value).convert_to<std::string>();
}

// Render raw atoms as a plain decimal with `places` fractional digits and
// trailing zeros removed. Native MIN uses 44; a token uses its metadata
// decimals (e.g. 8 -> 100000000 atoms displays as 1).
inline std::string formatAtoms(boost::multiprecision::cpp_int atoms, int places) {
    if (places < 0 || places > kAmountDecimals) places = kAmountDecimals;
    std::string digits = atoms.convert_to<std::string>();
    const bool negative = !digits.empty() && digits.front() == '-';
    if (negative) digits.erase(digits.begin());
    if (digits.size() <= static_cast<std::size_t>(places)) {
        digits.insert(0, static_cast<std::size_t>(places) + 1 - digits.size(), '0');
    }
    if (places > 0) {
        digits.insert(digits.size() - static_cast<std::size_t>(places), 1, '.');
        const std::size_t last = digits.find_last_not_of('0');
        if (last != std::string::npos && digits[last] == '.') {
            digits.erase(last);
        } else if (last != std::string::npos) {
            digits.erase(last + 1);
        }
    }
    if (negative && digits != "0") digits.insert(digits.begin(), '-');
    return digits;
}

inline std::string formatAtoms(boost::multiprecision::cpp_int atoms) {
    return formatAtoms(std::move(atoms), kAmountDecimals);
}

} // namespace minima::analytics
