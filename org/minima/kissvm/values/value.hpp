#pragma once

#include <memory>
#include <string>
#include <stdexcept>

namespace org {
namespace minima {
namespace kissvm {
namespace values {

class Value {
public:
    // The Only Value Types
    static constexpr int VALUE_HEX     = 1;
    static constexpr int VALUE_NUMBER  = 2;
    static constexpr int VALUE_SCRIPT  = 4;
    static constexpr int VALUE_BOOLEAN = 8;

    virtual ~Value() = default;
    virtual Value* clone() const = 0;

    // What type of Value is this..
    virtual int getValueType() const = 0;

    // Virtual toString so derived classes' overrides are valid
    virtual std::string toString() const;

    // Strict Check this type and throw an exception if not
    void verifyType(int zType) const;

    // GLOBAL STATIC FUNCTION for creating a Value from any string
    static std::unique_ptr<Value> getValue(const std::string& zValue);

    // GLOBAL STATIC FUNCTION for telling the value type
    static int getValueType(const std::string& zValue);

    // Get the type as a string
    static std::string getValueTypeString(int zType);

    // Check that both values are of the same type and throw an exception if not
    static void checkSameType(const Value& zV1, const Value& zV2);
    static void checkSameType(const Value& zV1, const Value& zV2, int zType);
};

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org