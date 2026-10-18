#ifndef ORG_MINIMA_KISSVM_VALUES_BOOLEAN_VALUE_HPP
#define ORG_MINIMA_KISSVM_VALUES_BOOLEAN_VALUE_HPP
#pragma once

#include <string>
#include "org/minima/kissvm/values/value.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace values {

class BooleanValue : public Value {
public:
    // Global True/False instances
    // --- DECLARE them as 'static const' (NO 'inline') ---
    static const BooleanValue FALSE;
    static const BooleanValue TRUE;

    explicit BooleanValue(bool zValue);

    // Virtual destructor for proper polymorphic cleanup
    virtual ~BooleanValue();

    // TRUE or FALSE
    bool isTrue() const;
    bool isFalse() const;

    // String representation: "TRUE" or "FALSE"
    std::string toString() const override;

    // Value type identifier
    int getValueType() const override;

    // Equality check with another BooleanValue
    bool isEqual(const BooleanValue& zBool) const;
    
    BooleanValue* clone() const override;


private:
    bool mTrue;
};

// --- DEFINE them as 'inline' HERE, after the class is complete ---
inline const BooleanValue BooleanValue::FALSE{false};
inline const BooleanValue BooleanValue::TRUE{true};

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org

#endif // ORG_MINIMA_KISSVM_VALUES_BOOLEAN_VALUE_HPP