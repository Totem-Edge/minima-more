#pragma once

#include "org/minima/utils/streamable.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace org {
namespace minima {
namespace objects {
namespace base {

class MiniNumber : public org::minima::utils::Streamable {
public:
    // Constants matching Java
    static constexpr int MAX_DIGITS = 64;
    static constexpr int MAX_DECIMAL_PLACES = MAX_DIGITS - 20; // 44

    // Constructors
    MiniNumber();                          // 0
    explicit MiniNumber(int zNumber);
    explicit MiniNumber(long long zNumber);
    explicit MiniNumber(const boost::multiprecision::cpp_int& zNumber); // integer value
    explicit MiniNumber(const std::string& zNumber); // decimal string (may include 'E' exponent)
    MiniNumber(const MiniNumber& other);

    // Assignment
    MiniNumber& operator=(const MiniNumber& other);

    // Query / conversion
    // Java getNumber() returns BigDecimal; here we return toPlainString
    std::string getNumber() const;
    // Java getAsBigDecimal(): return toPlainString equivalent
    std::string getAsBigDecimal() const;
    boost::multiprecision::cpp_int getAsBigInteger() const; // trunc toward zero
    long long getAsLong() const;  // 2's complement low 64 bits of integer part (Java-like)
    int getAsInt() const;         // 2's complement low 32 bits of integer part (Java-like)
    double getAsDouble() const;   // via decimal string parse

    // Arithmetic (return new MiniNumber with MathContext and limits applied)
    MiniNumber add(const MiniNumber& zNumber) const;
    MiniNumber sub(const MiniNumber& zNumber) const;
    MiniNumber div(const MiniNumber& zNumber) const;   // throws on divide-by-zero
    MiniNumber mult(const MiniNumber& zNumber) const;
    MiniNumber pow(int zNumber) const;                 // supports negative exponents
    MiniNumber sqrt() const;                           // throws on negative

    MiniNumber modulo(const MiniNumber& zNumber) const; // remainder toward-zero quotient
    MiniNumber floor() const;                           // setScale(0, FLOOR)
    MiniNumber ceil() const;                            // setScale(0, CEILING)
    MiniNumber setSignificantDigits(int zSignificantDigits) const; // RoundingMode.DOWN
    MiniNumber abs() const;
    MiniNumber increment() const;
    MiniNumber decrement() const;

    int decimalPlaces() const; // scale() in Java; can be negative

    // Exact multiply of a BigInteger by this MiniNumber.
    // Returns floor(BigInteger * this) to match Java BigDecimal multiply + toBigInteger truncation.
    boost::multiprecision::cpp_int multBigInteger(const boost::multiprecision::cpp_int& zValue) const;

    // Internal accessors (for exact arithmetic)
    const boost::multiprecision::cpp_int& getUnscaled() const;
    int getScale() const;

    // Comparison
    int compareTo(const MiniNumber& zCompare) const;
    bool isEqual(const MiniNumber& zNumber) const;
    bool isLess(const MiniNumber& zNumber) const;
    bool isLessEqual(const MiniNumber& zNumber) const;
    bool isMore(const MiniNumber& zNumber) const;
    bool isMoreEqual(const MiniNumber& zNumber) const;

    // Minima value validation
    bool isValidMinimaValue() const;

    // String
    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers (Java-style)
    static MiniNumber ReadFromStream(std::istream& in);
    static void WriteToStream(std::ostream& out, int zNumber);

    // Useful constants (matching Java)
    static const MiniNumber& MINI_UNIT(); // 1E- MAX_DECIMAL_PLACES
    static const MiniNumber& MAXIMUM(); // 2^64 - 1

    static const MiniNumber& ZERO();
    static const MiniNumber& ONE();
    static const MiniNumber& TWO();
    static const MiniNumber& THREE();
    static const MiniNumber& FOUR();
    static const MiniNumber& EIGHT();
    static const MiniNumber& TWELVE();
    static const MiniNumber& SIXTEEN();
    static const MiniNumber& TWENTY();
    static const MiniNumber& THIRTYTWO();
    static const MiniNumber& FIFTY();
    static const MiniNumber& SIXTYFOUR();
    static const MiniNumber& TWOFIVESIX();
    static const MiniNumber& FIVEONE12();
    static const MiniNumber& THOUSAND24();

    static const MiniNumber& TEN();
    static const MiniNumber& HUNDRED();
    static const MiniNumber& THOUSAND();
    static const MiniNumber& MILLION();
    static const MiniNumber& HUNDMILLION();
    static const MiniNumber& BILLION();
    static const MiniNumber& TRILLION();

    static const MiniNumber& MINUSONE();

private:
    // Internal representation: value = m_unscaled * 10^(-m_scale)
    boost::multiprecision::cpp_int m_unscaled;
    int m_scale;

    // Helpers
    static boost::multiprecision::cpp_int pow10(unsigned int n);
    static unsigned int count_decimal_digits(const boost::multiprecision::cpp_int& v); // for |v|
    static void align_scales(const MiniNumber& a, const MiniNumber& b,
                             boost::multiprecision::cpp_int& ua,
                             boost::multiprecision::cpp_int& ub,
                             int& scale); // make both to same 'scale' = max(a.scale,b.scale)

    void normalize_zero(); // if unscaled == 0 -> scale = 0
    void apply_precision_down(int precision); // limit sig digits to precision (<=MAX_DIGITS)
    void apply_math_context_default(); // precision = MAX_DIGITS
    void enforce_max_decimal_places(); // scale <= MAX_DECIMAL_PLACES rounding DOWN
    void checkLimits(); // enforce decimal places and range

    // setScale with rounding modes needed
    enum class RoundMode { DOWN, FLOOR, CEILING };
    MiniNumber setScale_impl(int newScale, RoundMode mode) const;

    // Division with extra precision then precision-round
    MiniNumber divide_with_context(const MiniNumber& other) const;

    // Modulo helper (toward-zero quotient)
    MiniNumber remainder_toward_zero(const MiniNumber& other) const;

    // Parsing and formatting
    static void parse_decimal_string(const std::string& s,
                                     boost::multiprecision::cpp_int& unscaled_out,
                                     int& scale_out);
    std::string to_plain_string_strip_trailing_zeros() const;

    // Two's complement import/export for serialization (big-endian)
    static std::vector<uint8_t> to_twos_complement_bytes(const boost::multiprecision::cpp_int& value);
    static boost::multiprecision::cpp_int from_twos_complement_bytes(const std::vector<uint8_t>& bytes);

    // Comparison helper (without sign shortcuts)
    static int compare_values(const boost::multiprecision::cpp_int& ua, int sa,
                              const boost::multiprecision::cpp_int& ub, int sb);
    
    // Add this enum
    enum class UncheckedCtor { k };
    // Add this private constructor overload
    MiniNumber(const boost::multiprecision::cpp_int& zNumber, UncheckedCtor);

    static const boost::multiprecision::cpp_int& get_max_value();
    static const boost::multiprecision::cpp_int& get_min_value();
};

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org