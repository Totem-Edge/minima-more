#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <istream>
#include <ostream>

#include "org/minima/utils/streamable.hpp"

namespace org {
namespace minima {
namespace objects {
namespace base {

class MiniString : public org::minima::utils::Streamable {
public:
    // Minima Charset - UTF-8 (for parity with Java; informational)
    static constexpr const char* MINIMA_CHARSET = "UTF-8";

private:
    std::string mString;

public:
    // Constructors
    MiniString(); // empty string
    explicit MiniString(const std::string& zString);
    explicit MiniString(const std::vector<uint8_t>& zBytesData);
    MiniString(const MiniString& zString);

    // Rule of 5 defaults
    MiniString& operator=(const MiniString&) = default;
    MiniString(MiniString&&) noexcept = default;
    MiniString& operator=(MiniString&&) noexcept = default;
    virtual ~MiniString();

    // Methods
    bool isEqual(const std::string& zString) const;
    std::string toString() const;
    std::vector<uint8_t> getData() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers
    static MiniString ReadFromStream(std::istream& in);
    static void WriteToStream(std::ostream& out, const std::string& zString);
};

} // namespace base
} // namespace objects
} // namespace minima
} // namespace org