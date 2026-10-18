#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// Forward declarations for project types used in members/signatures
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }

#include "org/minima/kissvm/values/value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace values {

class HexValue : public Value {
public:
    // Constructors
    explicit HexValue(const std::string& zHex);
    explicit HexValue(const org::minima::objects::base::MiniData& zData);
    explicit HexValue(const std::vector<std::uint8_t>& zData);
    explicit HexValue(const org::minima::objects::base::MiniNumber& zNumber);

    // Special members due to unique_ptr to forward-declared type
    virtual ~HexValue();
    HexValue(HexValue&&) noexcept;
    HexValue& operator=(HexValue&&) noexcept;

    // Delete copy operations
    HexValue(const HexValue&) = delete;
    HexValue& operator=(const HexValue&) = delete;

    // Accessors
    const org::minima::objects::base::MiniData& getMiniData() const;
    std::vector<std::uint8_t> getRawData() const;

    // Value interface
    int getValueType() const override;

    // Comparisons
    bool isEqual(const HexValue& zValue) const;

    // String
    std::string toString() const override;

    // Clone
    HexValue* clone() const override;

private:
    std::unique_ptr<org::minima::objects::base::MiniData> mData;

    // Helper to enforce max size constraint
    static void ensure_max_size_or_throw(int len);
};

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org