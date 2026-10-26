#include "org/minima/objects/base/mini_data.hpp"

#include <stdexcept>
#include <sstream>
#include <openssl/rand.h>
#include <algorithm>
#include <random>
#include <limits>
#include <cstring>
#include <iomanip>

#include "org/minima/utils/base_converter.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/mini_format.hpp"
#include <boost/multiprecision/cpp_int.hpp>

namespace org {
namespace minima {
namespace objects {
namespace base {

using boost::multiprecision::cpp_int;
using org::minima::utils::MinimaLogger;

// Static variable definitions
int MiniData::MINIMA_MAX_HASH_LENGTH = 64;
// 16 MB
int MiniData::MINIMA_MAX_MINIDATA_LENGTH = 1024 * 1024 * 512;

// Static constants
const MiniData& MiniData::ZERO_TXPOWID() {
    static const MiniData instance("0x00");
    return instance;
}
const MiniData& MiniData::ONE_TXPOWID() {
    static const MiniData instance("0x01");
    return instance;
}

// Constructors
MiniData::MiniData() : mData(), mDataValue(nullptr) {}

MiniData::MiniData(const std::string& zHexString) : mDataValue(nullptr) {
    mData = org::minima::utils::BaseConverter::decode16(zHexString);
}

MiniData::MiniData(const std::vector<std::uint8_t>& zData) : mData(zData), mDataValue(nullptr) {}

MiniData::MiniData(const MiniBuffer& zData) : mData(zData), mDataValue(nullptr) {}

MiniData::MiniData(const std::vector<std::uint8_t>& zData, int zMinLength) : mDataValue(nullptr) {
    int len = static_cast<int>(zData.size());
    if (len < zMinLength) {
        int diff = zMinLength - len;
        mData.assign(zMinLength, 0);
        if (len > 0) {
            std::copy(zData.begin(), zData.end(), mData.begin() + diff);
        }
    } else {
        mData = zData;
    }
}


MiniData::MiniData(const cpp_int& zBigInteger) {
    
    if (zBigInteger.sign() < 0) {
        throw std::invalid_argument("MiniData value must be a positive BigInteger");
    }
    
    // Export the absolute value as big-endian bytes
    boost::multiprecision::export_bits(zBigInteger, std::back_inserter(mData), 8);
    
    // Handle the case where the number is 0
    if (mData.empty() && zBigInteger == 0) {
        mData.push_back(0x00);
    }
    
    // Java's MiniData constructor removes a leading 0x00 byte if present
    if (mData.size() > 1 && mData[0] == 0x00) {
        mData.erase(mData.begin());
    }

    // Cache the value
    mDataValue = std::make_unique<cpp_int>(zBigInteger);
}


const cpp_int& MiniData::getValue() const {
    if (!mDataValue) {
        mDataValue = std::make_unique<cpp_int>();
        // Import as unsigned big-endian bytes
        // This mimics Java's new BigInteger(1, mData)
        if (!mData.empty()) {
            boost::multiprecision::import_bits(*mDataValue, mData.begin(), mData.end(), 8);
        }
    }
    return *mDataValue;
}


MiniData::MiniData(const MiniData& other)
    : mData(other.mData), // [1] Copy the byte vector
      mDataValue(nullptr)   // [2] Start with an empty cache
{
    // [3] If the other object's cache was full, copy its value
    if (other.mDataValue) {
        mDataValue = std::make_unique<cpp_int>(*other.mDataValue);
    }
}


MiniData& MiniData::operator=(const MiniData& other) {
    // [1] Protect against self-assignment (e.g., myData = myData)
    if (this == &other) {
        return *this;
    }
    
    // [2] Copy the byte vector
    mData = other.mData; 
    
    // [3] Copy the cached value, or reset our own cache if other's is empty
    if (other.mDataValue) {
        mDataValue = std::make_unique<cpp_int>(*other.mDataValue);
    } else {
        mDataValue.reset(nullptr); 
    }
    
    return *this;
}


// Accessors
int MiniData::getLength() const {
    return static_cast<int>(mData.size());
}

const MiniBuffer& MiniData::getBytes() const {
    return mData;
}

// Decimal (BigInteger-like)
std::string MiniData::getDataValue() const {
    // --- UPDATED TO USE CACHE ---
    return getValue().convert_to<std::string>();
}

double MiniData::getDataValueDecimal() const {
    // --- UPDATED TO USE CACHE ---
    return getValue().convert_to<double>();
}

// Comparisons
bool MiniData::isEqual(const MiniData& zCompare) const {
    // Match Java MiniData.java:145-164 exactly:
    // 1. Check length equality first
    int len = static_cast<int>(mData.size());
    if (len != static_cast<int>(zCompare.mData.size())) {
        return false;
    }
    // 2. Byte-by-byte comparison (no numeric/cpp_int fallback)
    for (int i = 0; i < len; i++) {
        if (mData[i] != zCompare.mData[i]) {
            return false;
        }
    }
    return true;
}

int MiniData::compare(const MiniData& zCompare) const {
    // --- UPDATED TO USE CACHE ---
    // Optimization: if both values are uncached, compare lengths first
    // This matches the logic in the original Java's BigInteger.compareTo
    if (!mDataValue && !zCompare.mDataValue) {
        MiniBuffer a = stripLeadingZeros(mData);
        MiniBuffer b = stripLeadingZeros(zCompare.mData);
        return compareMagnitude(a, b);
    }
    
    // Use cached values if available
    return getValue().compare(zCompare.getValue());
}

bool MiniData::isLess(const MiniData& zCompare) const {
    return compare(zCompare) < 0;
}

bool MiniData::isLessEqual(const MiniData& zCompare) const {
    return compare(zCompare) <= 0;
}

bool MiniData::isMore(const MiniData& zCompare) const {
    return compare(zCompare) > 0;
}

bool MiniData::isMoreEqual(const MiniData& zCompare) const {
    return compare(zCompare) >= 0;
}

// Shifts
MiniData MiniData::shiftr(int zNumber) const {
    // --- UPDATED TO USE CACHE ---
    if (zNumber <= 0) {
        return MiniData(mData);
    }
    cpp_int res = getValue() >> zNumber;
    return MiniData(res);
}

MiniData MiniData::shiftl(int zNumber) const {
    // --- UPDATED TO USE CACHE ---
    if (zNumber <= 0) {
        return MiniData(mData);
    }
    cpp_int res = getValue() << zNumber;
    return MiniData(res);
}

// Concat
MiniData MiniData::concat(const MiniData& zConcat) const {
    std::vector<std::uint8_t> total;
    total.reserve(mData.size() + zConcat.mData.size());
    total.insert(total.end(), mData.begin(), mData.end());
    total.insert(total.end(), zConcat.mData.begin(), zConcat.mData.end());
    return MiniData(total);
}

// Strings
std::string MiniData::toString() const {
    return to0xString();
}

std::string MiniData::to0xString() const {
    return org::minima::utils::BaseConverter::encode16(mData.data(), mData.size());
}

std::string MiniData::to0xString(int zLen) const {
    std::string data = to0xString();
    int len = static_cast<int>(data.size());
    if (len > zLen) {
        len = zLen;
    }
    return data.substr(0, static_cast<size_t>(len)).append("..");
}

// Streamable I/O
void MiniData::writeDataStream(std::ostream& out) {
    writeInt32BE(out, static_cast<std::int32_t>(mData.size()));
    if (!mData.empty()) {
        out.write(reinterpret_cast<const char*>(mData.data()), static_cast<std::streamsize>(mData.size()));
    }
    if (!out.good()) {
        throw std::ios_base::failure("Write Error : MiniData write failed");
    }
}

void MiniData::readDataStream(std::istream& in) {
    readDataStream(in, -1);
}

void MiniData::readDataStream(std::istream& in, int zSize) {
    std::int32_t len = readInt32BE(in);

    // Check against maximum allowed (error message mirrors Java)
    if (len > MINIMA_MAX_MINIDATA_LENGTH) {
        std::string msg = "Read Error : MiniData Length larger than maximum allowed (16 MB) ";
        msg += org::minima::utils::MiniFormat::formatSize(len);
        throw std::ios_base::failure(msg);
    }
    if (len < 0) {
        throw std::ios_base::failure("Read Error : MiniData Length less than 0");
    }
    if (zSize != -1) {
        if (len != zSize) {
            std::ostringstream oss;
            oss << "Read Error : MiniData length not correct as specified " << zSize;
            throw std::ios_base::failure(oss.str());
        }
    }

    mData.assign(static_cast<size_t>(len), 0);
    readFully(in, mData);
    mDataValue.reset(nullptr); // Invalidate cache
}

MiniData MiniData::ReadFromStream(std::istream& in) {
    return ReadFromStream(in, -1);
}

MiniData MiniData::ReadFromStream(std::istream& in, int zSize) {
    MiniData data;
    data.readDataStream(in, zSize);
    return data;
}

// HASH I/O
void MiniData::writeHashToStream(std::ostream& out) {
    if (static_cast<int>(mData.size()) > MINIMA_MAX_HASH_LENGTH) {
        std::ostringstream oss;
        oss << "Write Error : HASH Length greater than " << MINIMA_MAX_HASH_LENGTH << "! " << mData.size();
        throw std::ios_base::failure(oss.str());
    }
    writeInt32BE(out, static_cast<std::int32_t>(mData.size()));
    if (!mData.empty()) {
        out.write(reinterpret_cast<const char*>(mData.data()), static_cast<std::streamsize>(mData.size()));
    }
    if (!out.good()) {
        throw std::ios_base::failure("Write Error : HASH write failed");
    }
}

void MiniData::readHashFromStream(std::istream& in) {
    std::int32_t len = readInt32BE(in);
    if (len > MINIMA_MAX_HASH_LENGTH) {
        std::ostringstream oss;
        oss << "Read Error : HASH Length greater than " << MINIMA_MAX_HASH_LENGTH << "! " << len;
        throw std::ios_base::failure(oss.str());
    } else if (len < 0) {
        std::ostringstream oss;
        oss << "Read Error : HASH Length less than 0! " << len;
        throw std::ios_base::failure(oss.str());
    }

    mData.assign(static_cast<size_t>(len), 0);
    readFully(in, mData);
    mDataValue.reset(nullptr); // Invalidate cache
}

MiniData MiniData::ReadHashFromStream(std::istream& in) {
    MiniData data;
    data.readHashFromStream(in);
    return data;
}

void MiniData::WriteToStream(std::ostream& out, const std::vector<std::uint8_t>& zData) {
    MiniData tmp(zData);
    tmp.writeDataStream(out);
}

// Serialize a Streamable to MiniData
std::unique_ptr<MiniData> MiniData::getMiniDataVersion(org::minima::utils::Streamable& zObject) {
    std::ostringstream oss(std::ios::binary);
    try {
        zObject.writeDataStream(oss);
        oss.flush();
        std::string bytes = oss.str();
        std::vector<std::uint8_t> data(bytes.begin(), bytes.end());
        return std::make_unique<MiniData>(data);
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
    } catch (...) {
        org::minima::utils::MinimaLogger::log(std::string("Unknown exception in getMiniDataVersion"));
    }
    return nullptr;
}

// Random data
MiniData MiniData::getRandomData(int len) {
    if (len < 0) {
        throw std::invalid_argument("getRandomData length must be non-negative");
    }
    std::vector<std::uint8_t> data(static_cast<size_t>(len));

    // Cryptographically secure randomness (was mt19937): use the OS CSPRNG.
    if (len > 0 && RAND_bytes(data.data(), len) != 1) {
        throw std::runtime_error("RAND_bytes failed (MiniData::getRandomData)");
    }
    return MiniData(data);
}

// Private helpers
void MiniData::writeInt32BE(std::ostream& out, std::int32_t v) {
    std::uint8_t buf[4];
    buf[0] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    buf[1] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    buf[2] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    buf[3] = static_cast<std::uint8_t>(v & 0xFF);
    out.write(reinterpret_cast<const char*>(buf), 4);
}

std::int32_t MiniData::readInt32BE(std::istream& in) {
    std::uint8_t buf[4] = {0,0,0,0};
    in.read(reinterpret_cast<char*>(buf), 4);
    if (in.gcount() != 4) {
        throw std::ios_base::failure("Read Error : Unable to read 4 bytes for int length");
    }
    // Use uint32_t for shifts to avoid signed overflow UB (matching Java's defined wrap-around)
    std::uint32_t u = (static_cast<std::uint32_t>(buf[0]) << 24) |
                      (static_cast<std::uint32_t>(buf[1]) << 16) |
                      (static_cast<std::uint32_t>(buf[2]) << 8)  |
                      (static_cast<std::uint32_t>(buf[3]));
    return static_cast<std::int32_t>(u);
}

void MiniData::readFully(std::istream& in, MiniBuffer& buf) {
    if (buf.empty()) return;
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (in.gcount() != static_cast<std::streamsize>(buf.size())) {
        throw std::ios_base::failure("Read Error : Unable to read full byte array");
    }
}

MiniBuffer MiniData::stripLeadingZeros(const MiniBuffer& in) {
    size_t i = 0;
    while (i < in.size() && in[i] == 0) {
        ++i;
    }
    if (i == 0) {
        return in;
    }
    if (i >= in.size()) {
        // Return empty buffer if all zeros, but keep one '0' if it was originally "0x00"
        if (in.size() == 1 && in[0] == 0) {
             return MiniBuffer{0}; // Special case for "0x00"
        }
        return MiniBuffer{}; // All zeros
    }
    return MiniBuffer(in.data() + i, in.size() - i);
}

int MiniData::compareMagnitude(const MiniBuffer& a, const MiniBuffer& b) {
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    // Same length: lexicographic compare
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

// --- REMOVED UNUSED HELPERS ---
// shiftLeftBits
// shiftRightBits
// toDecimalString

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org
