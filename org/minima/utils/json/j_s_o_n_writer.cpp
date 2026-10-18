#include "org/minima/utils/json/j_s_o_n_writer.hpp"

namespace org {
namespace minima {
namespace utils {
namespace json {

JSONWriter::JSONWriter()
    : mBuffer() {
}

std::string JSONWriter::getFullString() const {
    return mBuffer;
}

void JSONWriter::write(const std::string& zString) {
    mBuffer.append(zString);
}

void JSONWriter::write(int zChar) {
    mBuffer.push_back(static_cast<char>(zChar));
}

std::string JSONWriter::toString() const {
    return mBuffer;
}

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org