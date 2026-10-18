#pragma once

#include "org/minima/utils/streamable.hpp"

#include <string>
#include <vector>
#include <cstdint>

#include <boost/multiprecision/cpp_int.hpp>

namespace org {
namespace minima {
namespace objects {
namespace mmr {

class MMREntryNumber : public org::minima::utils::Streamable {
public:
    // Constants
    static const MMREntryNumber ZERO;
    static const MMREntryNumber TWO;

    // Constructors
    MMREntryNumber();                       // 0
    explicit MMREntryNumber(int zNumber);   // from int (scale = 0)
    explicit MMREntryNumber(const boost::multiprecision::cpp_int& zBigInteger); // from BigInteger (scale = 0)

    // Accessor analogous to Java getBigDecimal(): returns plain decimal string
    std::string getBigDecimal() const;

    // Operations
    MMREntryNumber modulo(const MMREntryNumber& zNumber) const;
    MMREntryNumber floor() const;
    MMREntryNumber increment() const;
    MMREntryNumber decrement() const;
    MMREntryNumber div2() const;
    MMREntryNumber mult2() const;

    int compareTo(const MMREntryNumber& zCompare) const;
    bool isEqual(const MMREntryNumber& zNumber) const;
    bool isLess(const MMREntryNumber& zNumber) const;
    bool isLessEqual(const MMREntryNumber& zNumber) const;
    bool isMore(const MMREntryNumber& zNumber) const;
    bool isMoreEqual(const MMREntryNumber& zNumber) const;

    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helper matching Java API
    static MMREntryNumber ReadFromStream(std::istream& in);

private:
    // value = m_unscaled * 10^(-m_scale)
    boost::multiprecision::cpp_int m_unscaled;
    int m_scale;

    // Helpers
    static boost::multiprecision::cpp_int pow10(unsigned int n);

    static std::vector<std::uint8_t> to_twos_complement_bytes(const boost::multiprecision::cpp_int& value);
    static boost::multiprecision::cpp_int from_twos_complement_bytes(const std::vector<std::uint8_t>& bytes);

    static int compare_values(const boost::multiprecision::cpp_int& ua, int sa,
                              const boost::multiprecision::cpp_int& ub, int sb);

    static void align_to_scale(const boost::multiprecision::cpp_int& ua, int sa,
                               const boost::multiprecision::cpp_int& ub, int sb,
                               int targetScale,
                               boost::multiprecision::cpp_int& outA,
                               boost::multiprecision::cpp_int& outB);

    // Internal helpers for toString
    static std::string to_plain_string_strip_trailing_zeros(const boost::multiprecision::cpp_int& unscaled, int scale);
};

} // namespace mmr
} // namespace objects
} // namespace minima
} // namespace org