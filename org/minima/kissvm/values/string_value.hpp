#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

// Base class include (inheritance requires full type)
#include "org/minima/kissvm/values/value.hpp"

// Forward declaration for MiniString to avoid including full header in .hpp
namespace org { namespace minima { namespace objects { namespace base { class MiniString; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace values {

class StringValue : public Value {
private:
    // Store MiniString via unique_ptr to allow forward declaration
    std::unique_ptr<org::minima::objects::base::MiniString> mScript;

public:
    // Special member functions required due to unique_ptr to forward-declared type
    virtual ~StringValue();
    StringValue(StringValue&&) noexcept;
    StringValue& operator=(StringValue&&) noexcept;
    StringValue(const StringValue&) = delete;
    StringValue& operator=(const StringValue&) = delete;

    // Constructor
    explicit StringValue(const std::string& zScript);

    // Methods mirroring Java functionality
    std::string toString() const override;
    std::vector<uint8_t> getBytes() const;
    const org::minima::objects::base::MiniString& getMiniString() const;

    int getValueType() const override;

    bool isEqual(const StringValue& zValue) const;
    StringValue add(const StringValue& zSCValue) const;

    StringValue* clone() const override;
};

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org