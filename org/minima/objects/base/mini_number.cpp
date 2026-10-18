#include "org/minima/objects/base/mini_number.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "org/minima/utils/minima_logger.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <cctype>

using boost::multiprecision::cpp_int;
using boost::multiprecision::cpp_dec_float_100;
using org::minima::utils::MinimaLogger;

namespace org {
namespace minima {
namespace objects {
namespace base {

const boost::multiprecision::cpp_int& MiniNumber::get_max_value() {
    // Relaxed from 2^64 to 2^256 to support chain weights, MMR totals and difficulty-derived values
    // (block weights can be >> 2^64 for low-difficulty blocks). Only decimal places are now strictly limited for safety.
    static const cpp_int max_val = (cpp_int(1) << 256) - 1;
    return max_val;
}

const boost::multiprecision::cpp_int& MiniNumber::get_min_value() {
    // Note: We call get_max_value() to ensure it's also initialized
    // Match the relaxed MAX so MIN is -(2^256 - 1) for the same reasons (weights/totals).
    static const cpp_int min_val = -get_max_value();
    return min_val;
}

// Static constants (Construct on First Use idiom)
const MiniNumber& MiniNumber::MINI_UNIT() {
    static const MiniNumber instance(std::string("1E-") + std::to_string(MiniNumber::MAX_DECIMAL_PLACES));
    return instance;
}
const MiniNumber& MiniNumber::MAXIMUM() {
    // Use the private constructor to avoid checkLimits() during static init
    static const MiniNumber instance([](){
        cpp_int max = (cpp_int(1) << 64) - 1;
        return MiniNumber(max, MiniNumber::UncheckedCtor::k); 
    }());
    return instance;
}

const MiniNumber& MiniNumber::ZERO() { static const MiniNumber instance(0LL); return instance; }
const MiniNumber& MiniNumber::ONE() { static const MiniNumber instance(1LL); return instance; }
const MiniNumber& MiniNumber::TWO() { static const MiniNumber instance(2LL); return instance; }
const MiniNumber& MiniNumber::THREE() { static const MiniNumber instance(3LL); return instance; }
const MiniNumber& MiniNumber::FOUR() { static const MiniNumber instance(4LL); return instance; }
const MiniNumber& MiniNumber::EIGHT() { static const MiniNumber instance(8LL); return instance; }
const MiniNumber& MiniNumber::TWELVE() { static const MiniNumber instance(12LL); return instance; }
const MiniNumber& MiniNumber::SIXTEEN() { static const MiniNumber instance(16LL); return instance; }
const MiniNumber& MiniNumber::TWENTY() { static const MiniNumber instance(20LL); return instance; }
const MiniNumber& MiniNumber::THIRTYTWO() { static const MiniNumber instance(32LL); return instance; }
const MiniNumber& MiniNumber::FIFTY() { static const MiniNumber instance(50LL); return instance; }
const MiniNumber& MiniNumber::SIXTYFOUR() { static const MiniNumber instance(64LL); return instance; }
const MiniNumber& MiniNumber::TWOFIVESIX() { static const MiniNumber instance(256LL); return instance; }
const MiniNumber& MiniNumber::FIVEONE12() { static const MiniNumber instance(512LL); return instance; }
const MiniNumber& MiniNumber::THOUSAND24() { static const MiniNumber instance(1024LL); return instance; }

const MiniNumber& MiniNumber::TEN() { static const MiniNumber instance("1E1"); return instance; }
const MiniNumber& MiniNumber::HUNDRED() { static const MiniNumber instance("1E2"); return instance; }
const MiniNumber& MiniNumber::THOUSAND() { static const MiniNumber instance("1E3"); return instance; }
const MiniNumber& MiniNumber::MILLION() { static const MiniNumber instance("1E6"); return instance; }
const MiniNumber& MiniNumber::HUNDMILLION() { static const MiniNumber instance("1E8"); return instance; }
const MiniNumber& MiniNumber::BILLION() { static const MiniNumber instance("1E9"); return instance; }
const MiniNumber& MiniNumber::TRILLION() { static const MiniNumber instance("1E12"); return instance; }

const MiniNumber& MiniNumber::MINUSONE() { static const MiniNumber instance(-1LL); return instance; }

// Constructors
MiniNumber::MiniNumber() : m_unscaled(0), m_scale(0) {}

MiniNumber::MiniNumber(int zNumber) : m_unscaled(zNumber), m_scale(0) {
    apply_math_context_default();
    checkLimits();
}

MiniNumber::MiniNumber(long long zNumber) : m_unscaled(zNumber), m_scale(0) {
    apply_math_context_default();
    checkLimits();
}

MiniNumber::MiniNumber(const cpp_int& zNumber) : m_unscaled(zNumber), m_scale(0) {
    apply_math_context_default();
    checkLimits();
}

MiniNumber::MiniNumber(const std::string& zNumber) : m_unscaled(0), m_scale(0) {
    // Handle empty strings gracefully - default to zero
    if (zNumber.empty() || zNumber.find_first_not_of(" \t\r\n") == std::string::npos) {
        // MinimaLogger::log("[DEBUG MININUM] Empty string provided, defaulting to ZERO");
        m_unscaled = 0;
        m_scale = 0;
        return;
    }

    parse_decimal_string(zNumber, m_unscaled, m_scale);
    apply_math_context_default();
    checkLimits();
}

MiniNumber::MiniNumber(const MiniNumber& other) : m_unscaled(other.m_unscaled), m_scale(other.m_scale) {
    apply_math_context_default();
    checkLimits();
}

MiniNumber::MiniNumber(const cpp_int& zNumber, UncheckedCtor) : m_unscaled(zNumber), m_scale(0) {
    // This special constructor does NOT call checkLimits(), avoiding the bug.
    // It still must apply the math context.
    apply_math_context_default();
}

MiniNumber& MiniNumber::operator=(const MiniNumber& other) {
    if (this != &other) {
        m_unscaled = other.m_unscaled;
        m_scale = other.m_scale;
        apply_math_context_default();
        checkLimits();
    }
    return *this;
}

// Query / conversion
std::string MiniNumber::getNumber() const {
    return to_plain_string_strip_trailing_zeros();
}

std::string MiniNumber::getAsBigDecimal() const {
    return to_plain_string_strip_trailing_zeros();
}

cpp_int MiniNumber::getAsBigInteger() const {
    if (m_scale <= 0) {
        // value = m_unscaled * 10^{-m_scale} = m_unscaled * 10^{|m_scale|}
        cpp_int factor = pow10(static_cast<unsigned int>(-m_scale));
        return m_unscaled * factor;
    } else {
        cpp_int divisor = pow10(static_cast<unsigned int>(m_scale));
        // toward zero truncation
        return m_unscaled / divisor;
    }
}

long long MiniNumber::getAsLong() const {
    // Emulate BigDecimal.longValue(): integer part then BigInteger.longValue() (2's complement low 64 bits)
    cpp_int bi = getAsBigInteger();
    cpp_int mask = (cpp_int(1) << 64) - 1;
    cpp_int mod = bi & mask; // low 64 bits
    uint64_t u = mod.convert_to<uint64_t>();
    long long s = static_cast<long long>(u);
    return s;
}

int MiniNumber::getAsInt() const {
    cpp_int bi = getAsBigInteger();
    cpp_int mask = (cpp_int(1) << 32) - 1;
    cpp_int mod = bi & mask; // low 32 bits
    uint32_t u = mod.convert_to<uint32_t>();
    int s = static_cast<int>(u);
    return s;
}

double MiniNumber::getAsDouble() const {
    // Convert m_unscaled * 10^(-m_scale) directly to a high-precision float
    // This avoids the massive string allocation of to_plain_string...
    try {
        cpp_dec_float_100 x(m_unscaled);
        if (m_scale != 0) {
            cpp_dec_float_100 p10 = boost::multiprecision::pow(cpp_dec_float_100(10), -m_scale);
            x *= p10;
        }
        return x.convert_to<double>();
    } catch (...) {
        // Handle potential conversion errors (e.g., out of range)
        if (m_unscaled == 0) return 0.0;
        return m_unscaled > 0 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
    }
}

// Helper: pow10
cpp_int MiniNumber::pow10(unsigned int n) {
    static const cpp_int ten = 10;
    cpp_int res = 1;
    for (unsigned int i = 0; i < n; ++i) {
        res *= ten;
    }
    return res;
}

unsigned int MiniNumber::count_decimal_digits(const cpp_int& v) {
    cpp_int a = v;
    if (a < 0) a = -a;
    if (a == 0) return 1;
    // Fast path: values that fit in 64 bits (the common case - amounts, sums)
    // have at most 20 decimal digits, far below MAX_DIGITS (64). Count them
    // without converting the cpp_int to a decimal string.
    if (a <= std::numeric_limits<std::uint64_t>::max()) {
        std::uint64_t x = a.convert_to<std::uint64_t>();
        unsigned int digits = 1;
        while (x >= 10) {
            x /= 10;
            ++digits;
        }
        return digits;
    }
    std::ostringstream oss;
    oss << a;
    std::string s = oss.str();
    return static_cast<unsigned int>(s.size());
}

void MiniNumber::align_scales(const MiniNumber& a, const MiniNumber& b,
                              cpp_int& ua, cpp_int& ub, int& scale) {
    if (a.m_scale == b.m_scale) {
        ua = a.m_unscaled;
        ub = b.m_unscaled;
        scale = a.m_scale;
    } else if (a.m_scale > b.m_scale) {
        int diff = a.m_scale - b.m_scale;
        ua = a.m_unscaled;
        ub = b.m_unscaled * pow10(static_cast<unsigned int>(diff));
        scale = a.m_scale;
    } else {
        int diff = b.m_scale - a.m_scale;
        ua = a.m_unscaled * pow10(static_cast<unsigned int>(diff));
        ub = b.m_unscaled;
        scale = b.m_scale;
    }
}

void MiniNumber::normalize_zero() {
    if (m_unscaled == 0) {
        m_scale = 0;
    }
}

void MiniNumber::apply_precision_down(int precision) {
    if (precision <= 0) {
        // Reduce to zero
        m_unscaled = 0;
        m_scale = 0;
        return;
    }
    cpp_int absu = m_unscaled >= 0 ? m_unscaled : -m_unscaled;
    unsigned int digits = count_decimal_digits(absu);
    if (digits > static_cast<unsigned int>(precision)) {
        unsigned int k = digits - static_cast<unsigned int>(precision);
        cpp_int divisor = pow10(k);
        // towards zero
        m_unscaled /= divisor;
        m_scale -= static_cast<int>(k);
    }
    normalize_zero();
}

void MiniNumber::apply_math_context_default() {
    apply_precision_down(MAX_DIGITS);
}

void MiniNumber::enforce_max_decimal_places() {
    if (m_scale > MAX_DECIMAL_PLACES) {
        int k = m_scale - MAX_DECIMAL_PLACES;
        cpp_int divisor = pow10(static_cast<unsigned int>(k));
        // RoundingMode.DOWN -> toward zero truncation
        m_unscaled /= divisor;
        m_scale = MAX_DECIMAL_PLACES;
        normalize_zero();
    }
}

int MiniNumber::compare_values(const cpp_int& ua, int sa, const cpp_int& ub, int sb) {
    if (sa == sb) {
        if (ua < ub) return -1;
        if (ua > ub) return 1;
        return 0;
    }
    // scale to common
    cpp_int uaa, ubb;
    int scale;
    MiniNumber tmpa, tmpb;
    tmpa.m_unscaled = ua; tmpa.m_scale = sa;
    tmpb.m_unscaled = ub; tmpb.m_scale = sb;
    align_scales(tmpa, tmpb, uaa, ubb, scale);
    if (uaa < ubb) return -1;
    if (uaa > ubb) return 1;
    return 0;
}

void MiniNumber::checkLimits() {
    enforce_max_decimal_places();

    int cmpMax = compare_values(m_unscaled, m_scale, get_max_value(), 0);
    if (cmpMax > 0) {
        throw std::runtime_error(std::string("MiniNumber too large - outside allowed range 2^64 ") + toString());
    }

    // Compare to MIN_VALUE (scale 0)
    int cmpMin = compare_values(m_unscaled, m_scale, get_min_value(), 0);
    if (cmpMin < 0) {
        throw std::runtime_error("MiniNumber too small - outside allowed range -(2^64)");
    }
}

// setScale helper with modes
MiniNumber MiniNumber::setScale_impl(int newScale, RoundMode mode) const {
    MiniNumber res = *this;
    if (newScale == res.m_scale) {
        return res;
    } else if (newScale > res.m_scale) {
        // increasing scale -> multiply unscaled by 10^(newScale - m_scale)
        int diff = newScale - res.m_scale;
        res.m_unscaled *= pow10(static_cast<unsigned int>(diff));
        res.m_scale = newScale;
    } else {
        // decreasing scale -> divide by 10^(m_scale - newScale) with rounding
        int diff = res.m_scale - newScale; // >0
        cpp_int divisor = pow10(static_cast<unsigned int>(diff));
        cpp_int q = res.m_unscaled / divisor; // trunc toward zero
        cpp_int r = res.m_unscaled % divisor;
        if (mode == RoundMode::DOWN) {
            // toward zero
        } else if (mode == RoundMode::FLOOR) {
            if (r != 0 && res.m_unscaled < 0) {
                q -= 1;
            }
        } else if (mode == RoundMode::CEILING) {
            if (r != 0 && res.m_unscaled > 0) {
                q += 1;
            }
        }
        res.m_unscaled = q;
        res.m_scale = newScale;
    }
    res.normalize_zero();
    return res;
}

// Arithmetic
MiniNumber MiniNumber::add(const MiniNumber& zNumber) const {
    cpp_int ua, ub;
    int sc;
    align_scales(*this, zNumber, ua, ub, sc);
    MiniNumber res;
    res.m_unscaled = ua + ub;
    res.m_scale = sc;
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::sub(const MiniNumber& zNumber) const {
    cpp_int ua, ub;
    int sc;
    align_scales(*this, zNumber, ua, ub, sc);
    MiniNumber res;
    res.m_unscaled = ua - ub;
    res.m_scale = sc;
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::divide_with_context(const MiniNumber& other) const {
    if (other.m_unscaled == 0) {
        throw std::runtime_error("Division by zero");
    }
    // value = (U1 * 10^{-S1}) / (U2 * 10^{-S2}) = (U1 * 10^{S2 + t}) / U2 * 10^{-(S1 + t)}
    const int t = MAX_DIGITS + 2; // extra precision before rounding
    cpp_int N = m_unscaled * pow10(static_cast<unsigned int>(other.m_scale + t));
    cpp_int Q = N / other.m_unscaled; // toward zero
    MiniNumber res;
    res.m_unscaled = Q;
    res.m_scale = m_scale + t;
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::div(const MiniNumber& zNumber) const {
    return divide_with_context(zNumber);
}

MiniNumber MiniNumber::mult(const MiniNumber& zNumber) const {
    MiniNumber res;
    res.m_unscaled = m_unscaled * zNumber.m_unscaled;
    res.m_scale = m_scale + zNumber.m_scale;
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

cpp_int MiniNumber::multBigInteger(const cpp_int& zValue) const {
    // Match Java: BigDecimal avg.multiply(speedratio).toBigInteger()
    // speedratio = m_unscaled * 10^(-m_scale)
    // result = zValue * m_unscaled / 10^m_scale  (truncated toward zero)
    cpp_int prod = zValue * m_unscaled;
    if (m_scale <= 0) {
        if (m_scale < 0) {
            prod *= pow10(static_cast<unsigned int>(-m_scale));
        }
        return prod;
    }
    cpp_int div = pow10(static_cast<unsigned int>(m_scale));
    return prod / div; // cpp_int division truncates toward zero, matching Java toBigInteger()
}

const cpp_int& MiniNumber::getUnscaled() const {
    return m_unscaled;
}

int MiniNumber::getScale() const {
    return m_scale;
}

MiniNumber MiniNumber::pow(int zNumber) const {
    if (zNumber == 0) {
        return ONE();
    } else if (zNumber < 0) {
        MiniNumber pos = this->pow(-zNumber);
        return ONE().div(pos);
    }
    // fast exponentiation
    MiniNumber base = *this;
    MiniNumber result = ONE();
    int e = zNumber;
    while (e > 0) {
        if (e & 1) {
            result = result.mult(base);
        }
        e >>= 1;
        if (e) {
            base = base.mult(base);
        }
    }
    // result already checked in mult
    result.apply_math_context_default();
    result.checkLimits();
    return result;
}

MiniNumber MiniNumber::sqrt() const {
    if (m_unscaled < 0) {
        throw std::runtime_error("Square root of negative number");
    }
    // Use high-precision float for sqrt, then round-down to 64 digits
    // x = m_unscaled * 10^{-m_scale}
    cpp_dec_float_100 x = cpp_dec_float_100(m_unscaled.convert_to<std::string>());
    if (m_scale != 0) {
        // multiply by 10^{-m_scale}
        cpp_dec_float_100 p10 = boost::multiprecision::pow(cpp_dec_float_100(10), -m_scale);
        x *= p10;
    }
    cpp_dec_float_100 s = boost::multiprecision::sqrt(x);
    // Convert to string with extra precision then parse and round-down
    std::string sstr = s.str(MAX_DIGITS + 10, std::ios_base::fmtflags(0)); // may be scientific; parser handles exponent
    MiniNumber res(sstr);
    // Constructor applies precision and limits already
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::remainder_toward_zero(const MiniNumber& other) const {
    if (other.m_unscaled == 0) {
        throw std::runtime_error("Modulo by zero");
    }
    // Align scales and compute q = trunc(ua/ub), r = this - q*other
    cpp_int ua, ub;
    int sc;
    align_scales(*this, other, ua, ub, sc);
    cpp_int q = ua / ub; // trunc toward zero
    // r_value = (ua - q*ub) * 10^{-sc}
    MiniNumber qmul;
    qmul.m_unscaled = q * ub;
    qmul.m_scale = sc;
    MiniNumber r;
    r.m_unscaled = ua - q * ub;
    r.m_scale = sc;
    r.apply_math_context_default();
    r.checkLimits();
    return r;
}

MiniNumber MiniNumber::modulo(const MiniNumber& zNumber) const {
    MiniNumber res = remainder_toward_zero(zNumber);
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::floor() const {
    MiniNumber res = setScale_impl(0, RoundMode::FLOOR);
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::ceil() const {
    MiniNumber res = setScale_impl(0, RoundMode::CEILING);
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::setSignificantDigits(int zSignificantDigits) const {
    if (zSignificantDigits > MAX_DIGITS) {
        throw std::invalid_argument("Cannot specify this many significant digits " + std::to_string(zSignificantDigits));
    } else if (zSignificantDigits < 0) {
        throw std::invalid_argument("Cannot specify negative significant digits " + std::to_string(zSignificantDigits));
    }
    MiniNumber res = *this;
    res.apply_precision_down(zSignificantDigits);
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::abs() const {
    MiniNumber res = *this;
    if (res.m_unscaled < 0) res.m_unscaled = -res.m_unscaled;
    res.apply_math_context_default();
    res.checkLimits();
    return res;
}

MiniNumber MiniNumber::increment() const {
    return this->add(ONE());
}

MiniNumber MiniNumber::decrement() const {
    return this->sub(ONE());
}

int MiniNumber::decimalPlaces() const {
    return m_scale;
}

int MiniNumber::compareTo(const MiniNumber& zCompare) const {
    // First, compare signs quickly
    bool negA = (m_unscaled < 0);
    bool negB = (zCompare.m_unscaled < 0);
    if (negA != negB) {
        return negA ? -1 : 1;
    }
    // Same sign: compare absolute values
    cpp_int ua = m_unscaled, ub = zCompare.m_unscaled;
    if (ua < 0) ua = -ua;
    if (ub < 0) ub = -ub;
    int cmp = compare_values(ua, m_scale, ub, zCompare.m_scale);
    if (negA && negB) {
        // reverse for negatives
        cmp = -cmp;
    }
    // For exact zero handling ensure equality if both unscaled == 0
    if (m_unscaled == 0 && zCompare.m_unscaled == 0) return 0;
    return cmp;
}

bool MiniNumber::isEqual(const MiniNumber& zNumber) const {
    return compareTo(zNumber) == 0;
}

bool MiniNumber::isLess(const MiniNumber& zNumber) const {
    return compareTo(zNumber) < 0;
}

bool MiniNumber::isLessEqual(const MiniNumber& zNumber) const {
    return compareTo(zNumber) <= 0;
}

bool MiniNumber::isMore(const MiniNumber& zNumber) const {
    return compareTo(zNumber) > 0;
}

bool MiniNumber::isMoreEqual(const MiniNumber& zNumber) const {
    return compareTo(zNumber) >= 0;
}

bool MiniNumber::isValidMinimaValue() const {
    return isLessEqual(MiniNumber::BILLION()) && isMore(MiniNumber::ZERO());
}

std::string MiniNumber::toString() const {
    return to_plain_string_strip_trailing_zeros();
}

// Streamable
void MiniNumber::writeDataStream(std::ostream& out) {
    // scale as signed byte
    int8_t scale_byte = static_cast<int8_t>(m_scale);
    out.write(reinterpret_cast<const char*>(&scale_byte), 1);

    // unscaled as two's complement minimal big-endian bytes
    std::vector<uint8_t> bytes = to_twos_complement_bytes(m_unscaled);
    // length as signed byte (Java writeByte). We assume length fits in 0..127
    int8_t len_byte = static_cast<int8_t>(bytes.size());
    out.write(reinterpret_cast<const char*>(&len_byte), 1);
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!out) {
        throw std::ios_base::failure("MiniNumber writeDataStream failed");
    }
}

void MiniNumber::readDataStream(std::istream& in) {
    // Read signed byte scale
    int8_t scale_byte = 0;
    in.read(reinterpret_cast<char*>(&scale_byte), 1);
    if (!in) throw std::ios_base::failure("MiniNumber readDataStream: failed to read scale");

    int8_t len_byte = 0;
    in.read(reinterpret_cast<char*>(&len_byte), 1);
    if (!in) throw std::ios_base::failure("MiniNumber readDataStream: failed to read length");

    int len = static_cast<int>(len_byte);
    if (len < 0 || len > 32) {
        throw std::ios_base::failure("ERROR reading MiniNumber - input too large or negative " + std::to_string(len));
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(len));
    if (len > 0) {
        in.read(reinterpret_cast<char*>(bytes.data()), len);
        if (!in) throw std::ios_base::failure("MiniNumber readDataStream: failed to read data");
    }

    m_scale = static_cast<int>(scale_byte);
    m_unscaled = from_twos_complement_bytes(bytes);

    // Apply MathContext(64, DOWN) as per Java constructor BigDecimal(unscaled, scale, MATH_CONTEXT)
    apply_math_context_default();
    // IMPORTANT: Java read does not call checkLimits explicitly; we mirror that behavior here.
}

// Static helpers
MiniNumber MiniNumber::ReadFromStream(std::istream& in) {
    // DEBUG
    // signed char scale_byte, length_byte;
    // in.read(reinterpret_cast<char*>(&scale_byte), 1);
    // in.read(reinterpret_cast<char*>(&length_byte), 1);
    
    // std::cout << "  [DEBUG MININUM] scale=" << (int)scale_byte 
    //           << " len=" << (int)length_byte 
    //           << " pos=" << in.tellg() << std::endl;
    // DEBUG END

    MiniNumber data;
    data.readDataStream(in);
    return data;
}

void MiniNumber::WriteToStream(std::ostream& out, int zNumber) {
    MiniNumber tmp(zNumber);
    tmp.writeDataStream(out);
}

// Parsing / formatting
void MiniNumber::parse_decimal_string(const std::string& s,
                                      cpp_int& unscaled_out,
                                      int& scale_out) {
    // Parse optional sign, digits with optional '.', optional exponent 'e' or 'E'
    // MinimaLogger::log("[DEBUG MININUM] parse_decimal_string: input='" + s + "'");
    std::string str = s;
    // Trim spaces
    auto ltrim = [](std::string& x){ x.erase(x.begin(), std::find_if(x.begin(), x.end(), [](unsigned char ch){ return !std::isspace(ch); })); };
    auto rtrim = [](std::string& x){ x.erase(std::find_if(x.rbegin(), x.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), x.end()); };
    ltrim(str); rtrim(str);
    if (str.empty()) {
        // MinimaLogger::log("[DEBUG MININUM] Empty after trim, original='" + s + "'");
        throw std::invalid_argument("Invalid decimal string: empty");
    }

    bool neg = false;
    size_t pos = 0;
    if (str[pos] == '+' || str[pos] == '-') {
        neg = (str[pos] == '-');
        ++pos;
    }

    // Split exponent if present
    size_t epos = str.find_first_of("eE", pos);
    std::string mantissa = (epos == std::string::npos) ? str.substr(pos) : str.substr(pos, epos - pos);
    int exponent = 0;
    if (epos != std::string::npos) {
        std::string expstr = str.substr(epos + 1);
        try {
            exponent = std::stoi(expstr);
        } catch (...) {
            throw std::invalid_argument("Invalid exponent in decimal string: " + s);
        }
    }

    // Process mantissa
    std::string digits;
    int frac_digits = 0;
    bool dot_seen = false;
    for (char c : mantissa) {
        if (c == '.') {
            if (dot_seen) throw std::invalid_argument("Invalid decimal string (multiple dots): " + s);
            dot_seen = true;
            continue;
        }
        if (c < '0' || c > '9') {
            throw std::invalid_argument("Invalid decimal digit in string: " + s);
        }
        digits.push_back(c);
        if (dot_seen) frac_digits++;
    }

    // Remove leading zeros in digits
    size_t first_nonzero = 0;
    while (first_nonzero < digits.size() && digits[first_nonzero] == '0') {
        first_nonzero++;
    }
    std::string core = digits.substr(first_nonzero);
    if (core.empty()) {
        // Zero
        unscaled_out = 0;
        scale_out = 0;
        return;
    }

    // Compute scale = frac_digits - exponent adjustment (note: "1E2" -> exponent=2 reduces scale)
    int scale = frac_digits - exponent;

    // Build unscaled
    cpp_int unscaled = 0;
    for (char c : core) {
        unscaled *= 10;
        unscaled += cpp_int(static_cast<int>(c - '0'));
    }
    if (neg) unscaled = -unscaled;

    unscaled_out = unscaled;
    scale_out = scale;
}

std::string MiniNumber::to_plain_string_strip_trailing_zeros() const {
    if (m_unscaled == 0) return "0";

    cpp_int absu = m_unscaled >= 0 ? m_unscaled : -m_unscaled;
    std::ostringstream oss;
    oss << absu;
    std::string digs = oss.str();
    int s = m_scale;

    std::string result;
    if (s >= 0) {
        // Insert decimal point if needed
        if (static_cast<int>(digs.size()) > s) {
            size_t ipos = digs.size() - static_cast<size_t>(s);
            std::string intpart = digs.substr(0, ipos);
            std::string frac = digs.substr(ipos);
            // Strip trailing zeros in fractional
            size_t end = frac.size();
            while (end > 0 && frac[end - 1] == '0') --end;
            if (end == 0) {
                result = intpart;
            } else {
                result = intpart + "." + frac.substr(0, end);
            }
        } else {
            // 0.xxx with leading zeros in fraction
            std::string frac;
            frac.reserve(static_cast<size_t>(s));
            int zeros = s - static_cast<int>(digs.size());
            frac.assign(static_cast<size_t>(zeros), '0');
            frac += digs;
            // Strip trailing zeros -> becomes "0" if all zeros (but digs not zero here)
            size_t end = frac.size();
            while (end > 0 && frac[end - 1] == '0') --end;
            if (end == 0) {
                result = "0";
            } else {
                result = "0." + frac.substr(0, end);
            }
        }
    } else {
        // s < 0 -> append zeros on the right
        std::string intpart = digs;
        int zeros = -s;
        intpart.append(static_cast<size_t>(zeros), '0');
        result = intpart;
    }

    if (m_unscaled < 0 && result != "0") {
        result.insert(result.begin(), '-');
    }
    return result;
}

// Two's complement serialization helpers
std::vector<uint8_t> MiniNumber::to_twos_complement_bytes(const cpp_int& value) {
    std::vector<uint8_t> out;
    if (value == 0) {
        out.push_back(0x00);
        return out;
    }
    if (value > 0) {
        // Export magnitude
        std::vector<uint8_t> mag;
        export_bits(value, std::back_inserter(mag), 8, /*big_endian=*/true);
        // Ensure sign bit not set; if set, prepend 0x00
        if (!mag.empty() && (mag[0] & 0x80)) {
            out.push_back(0x00);
        }
        out.insert(out.end(), mag.begin(), mag.end());
        return out;
    } else {
        cpp_int absval = -value;
        // Determine minimal byte width b = ceil((nbits + 1)/8)
        unsigned int nbits = msb(absval) + 1; // msb returns index of highest set bit
        unsigned int b = (nbits + 1 + 7) / 8;
        // Compute T = 2^(8b) - absval
        cpp_int M = cpp_int(1);
        M <<= (8 * b);
        cpp_int T = M - absval;
        // Export T to exactly b bytes
        std::vector<uint8_t> tmp;
        export_bits(T, std::back_inserter(tmp), 8, /*big_endian=*/true);
        // Left pad with 0x00 to reach b bytes if needed (unlikely)
        if (tmp.size() < b) {
            std::vector<uint8_t> pad(b - tmp.size(), 0x00);
            out.insert(out.end(), pad.begin(), pad.end());
        }
        out.insert(out.end(), tmp.begin(), tmp.end());
        // Ensure the top bit is 1 (negative). If not, prepend 0xFF (shouldn't be necessary with our b)
        if (!out.empty() && !(out[0] & 0x80)) {
            out.insert(out.begin(), 0xFF);
        }
        return out;
    }
}

boost::multiprecision::cpp_int MiniNumber::from_twos_complement_bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return cpp_int(0);
    bool negative = (bytes[0] & 0x80) != 0;
    // Import big-endian
    cpp_int val = 0;
    for (uint8_t b : bytes) {
        val <<= 8;
        val += cpp_int(b);
    }
    if (!negative) {
        return val;
    } else {
        // value = val - 2^(8*len)
        cpp_int M = cpp_int(1);
        M <<= (8 * bytes.size());
        return val - M;
    }
}

// Modulo etc already implemented above

// String override done above

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org