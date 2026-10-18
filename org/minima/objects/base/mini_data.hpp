#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_buffer.hpp"
#include <boost/multiprecision/cpp_int.hpp> // <-- ADDED THIS INCLUDE

namespace org {
namespace minima {
namespace objects {
namespace base {

class MiniData : public org::minima::utils::Streamable {
public:
    // Limits
    static int MINIMA_MAX_HASH_LENGTH;        // default 64
    static int MINIMA_MAX_MINIDATA_LENGTH;    // default 512 MB

    // Common constants
    static const MiniData& ZERO_TXPOWID();
    static const MiniData& ONE_TXPOWID();

    // Constructors
    MiniData();                                      // empty (length 0)
    explicit MiniData(const std::string& zHexString); // from hex string (e.g. "0x00" or "00")
    explicit MiniData(const std::vector<std::uint8_t>& zData); // from bytes
    MiniData(const std::vector<std::uint8_t>& zData, int zMinLength); // left-pad to min length with zeros
    explicit MiniData(const MiniBuffer& zData); // from SBO buffer (no copy)
    
    MiniData(const MiniData& other);
    MiniData& operator=(const MiniData& other);
    
    // --- ADDED THIS CONSTRUCTOR ---
    explicit MiniData(const boost::multiprecision::cpp_int& zBigInteger);

    // Basic accessors
    int getLength() const;
    const MiniBuffer& getBytes() const;

    // Decimal value (as Java BigInteger would print). Unsigned magnitude.
    std::string getDataValue() const;
    double getDataValueDecimal() const;

    // Equality and comparisons (unsigned big-endian magnitude)
    bool isEqual(const MiniData& zCompare) const;
    bool operator==(const MiniData& other) const { return isEqual(other); }
    bool isLess(const MiniData& zCompare) const;
    bool isLessEqual(const MiniData& zCompare) const;
    bool isMore(const MiniData& zCompare) const;
    bool isMoreEqual(const MiniData& zCompare) const;
    int compare(const MiniData& zCompare) const;

    // Bit shifts (like Java BigInteger.shiftLeft/Right)
    MiniData shiftr(int zNumber) const;
    MiniData shiftl(int zNumber) const;

    // Concatenate bytes (this || zConcat)
    MiniData concat(const MiniData& zConcat) const;

    // String representations
    std::string toString() const;
    std::string to0xString() const;
    std::string to0xString(int zLen) const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Read with explicit size guard
    void readDataStream(std::istream& in, int zSize);

    // Special HASH stream I/O with hash length limit
    void writeHashToStream(std::ostream& out);
    void readHashFromStream(std::istream& in);

    // Static helpers matching Java API intent
    static MiniData ReadFromStream(std::istream& in);
    static MiniData ReadFromStream(std::istream& in, int zSize);
    static MiniData ReadHashFromStream(std::istream& in);
    static void WriteToStream(std::ostream& out, const std::vector<std::uint8_t>& zData);

    // Serialize a Streamable to MiniData. Returns nullptr on error (Java returned null).
    static std::unique_ptr<MiniData> getMiniDataVersion(org::minima::utils::Streamable& zObject);

    // Random data
    static MiniData getRandomData(int len);

private:
    MiniBuffer mData;

    // --- ADDED CACHING AND HELPER ---
    mutable std::unique_ptr<boost::multiprecision::cpp_int> mDataValue;
    const boost::multiprecision::cpp_int& getValue() const;
    
    // Helpers
    static void writeInt32BE(std::ostream& out, std::int32_t v);
    static std::int32_t readInt32BE(std::istream& in);
    static void readFully(std::istream& in, MiniBuffer& buf);

    static MiniBuffer stripLeadingZeros(const MiniBuffer& in);
    static int compareMagnitude(const MiniBuffer& a, const MiniBuffer& b);
    // --- REMOVED UNUSED HELPERS ---
    // static std::vector<std::uint8_t> shiftLeftBits(const std::vector<std::uint8_t>& data, int bits);
    // static std::vector<std::uint8_t> shiftRightBits(const std::vector<std::uint8_t>& data, int bits);
    // static std::string toDecimalString(const std::vector<std::uint8_t>& data);
};

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org
