#include "org/minima/objects/base/mini_string.hpp"

#include <stdexcept>
#include <limits>
#include <cstring>

#include "org/minima/objects/base/mini_data.hpp"


namespace org {
namespace minima {
namespace objects {
namespace base {

MiniString::MiniString() : mString() {}

MiniString::MiniString(const std::string& zString)
    : mString(zString) {}  // Direct assignment, no sanitization

MiniString::MiniString(const std::vector<uint8_t>& zBytesData)
    : mString(std::string(
        reinterpret_cast<const char*>(zBytesData.data()),
        zBytesData.size())) {}  // Direct conversion

MiniString::MiniString(const MiniString& zString)
    : mString(zString.toString()) {}

MiniString::~MiniString() = default;

bool MiniString::isEqual(const std::string& zString) const {
    return mString == zString;
}

std::string MiniString::toString() const {
    return mString;
}

std::vector<uint8_t> MiniString::getData() const {
    const auto* dataPtr = reinterpret_cast<const unsigned char*>(mString.data());
    return std::vector<uint8_t>(dataPtr, dataPtr + mString.size());
}

void MiniString::writeDataStream(std::ostream& out) {
    // Create MiniData wrapper exactly like Java does
    MiniData strdata(getData());
    // Delegate serialization to MiniData
    strdata.writeDataStream(out);
}

void MiniString::readDataStream(std::istream& in) {
    MiniData strdata = MiniData::ReadFromStream(in);
    const std::vector<uint8_t>& bytes = strdata.getBytes();
    if (!bytes.empty()) {
        mString = std::string(
            reinterpret_cast<const char*>(bytes.data()),
            bytes.size()
        );  // Direct conversion without sanitization
    } else {
        mString.clear();
    }
}

MiniString MiniString::ReadFromStream(std::istream& in) {
    MiniString data;
    data.readDataStream(in);
    return data;
}

void MiniString::WriteToStream(std::ostream& out, const std::string& zString) {
    MiniString tmp(zString);
    tmp.writeDataStream(out);
}

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org
