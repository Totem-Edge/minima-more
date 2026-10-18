#pragma once

#include <cstdint>
#include <string>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"

namespace org {
namespace minima {
namespace objects {
namespace base {

class MiniByte : public org::minima::utils::Streamable {
public:
    // Global True/False singletons (Java: public static final)
    static const MiniByte& FALSE();
    static const MiniByte& TRUE();

    // Constructors
    MiniByte();                    // default 0
    explicit MiniByte(int val);    // cast to byte like Java (byte)val
    explicit MiniByte(int8_t zVal);
    explicit MiniByte(bool zVal);  // 1 for true, 0 for false

    // Accessors
    int getValue() const;          // 0..255 (mVal & 0xFF)
    int8_t getByteValue() const;   // raw signed byte

    // Comparisons
    bool isEqual(const MiniByte& zRamByte) const;
    bool isFalse() const;
    bool isTrue() const;

    // String representation (decimal of getValue())
    std::string toString() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers
    static MiniByte ReadFromStream(std::istream& in);
    static void WriteToStream(std::ostream& out, bool zData);

private:
    int8_t mVal;
};

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org