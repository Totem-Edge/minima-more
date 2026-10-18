#pragma once

#include <string>

namespace org {
namespace minima {
namespace utils {
namespace json {

class JSONWriter {
public:
    JSONWriter();

    // Returns the full accumulated string.
    std::string getFullString() const;

    // Appends the provided string to the internal buffer.
    void write(const std::string& zString);

    // Appends a single character, obtained by casting the int to char.
    void write(int zChar);

    // Returns the full accumulated string (equivalent to getFullString()).
    std::string toString() const;

private:
    std::string mBuffer;
};

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org