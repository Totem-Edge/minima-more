#pragma once

#include <string>

namespace org { namespace minima { namespace objects { namespace base {
class MiniData;
}}}}

namespace org { namespace minima { namespace system { namespace params {
class GlobalParams;
}}}}

namespace org {
namespace minima {
namespace utils {

class Maths {
public:
    // Java BigInteger("2") and BigDecimal("2") equivalents
    static const std::string BI_TWO;      // exact decimal string "2"
    static constexpr double BD_TWO = 2.0; // double 2.0

    // log2 for double, identical formula to Java: log10(z)/log10(2)
    static double log2(double zDD);

    // Get the Cascade Level of this Hash Value.. in comparison to what it needed to be
    static int getSuperLevel(const org::minima::objects::base::MiniData& zDifficulty,
                             const org::minima::objects::base::MiniData& zActual);

    // Log2 for big integer value represented by MiniData (functional replacement for Java BigInteger variant)
    static double log2BI(const org::minima::objects::base::MiniData& val);

    // Compare 2 version strings "x.y.z"
    static int compareVersions(const std::string& version1, const std::string& version2);
};

} // namespace utils
} // namespace minima
} // namespace org