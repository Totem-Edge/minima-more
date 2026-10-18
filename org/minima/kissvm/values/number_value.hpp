#pragma once

#include <string>

#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace values {

class NumberValue : public Value {
public:
    // Constructors
    explicit NumberValue(int zValue);
    explicit NumberValue(long long zValue);
    explicit NumberValue(const org::minima::objects::base::MiniNumber& zValue);
    explicit NumberValue(const std::string& zNumber);

    // Accessor (Java returns by value)
    org::minima::objects::base::MiniNumber getNumber() const;

    // Value overrides
    int getValueType() const override;

    // Predicates
    bool isFalse() const;

    // Comparisons
    bool isEqual(const NumberValue& zValue) const;
    bool isLess(const NumberValue& zValue) const;
    bool isLessEqual(const NumberValue& zValue) const;
    bool isMore(const NumberValue& zValue) const;
    bool isMoreEqual(const NumberValue& zValue) const;

    // Arithmetic
    NumberValue add(const NumberValue& zValue) const;
    NumberValue sub(const NumberValue& zValue) const;
    NumberValue mult(const NumberValue& zValue) const;
    NumberValue div(const NumberValue& zValue) const;

    // String
    std::string toString() const override;

    // clone
    NumberValue* clone() const override;

protected:
    org::minima::objects::base::MiniNumber mNumber;
};

} // namespace values
} // namespace kissvm
} // namespace minima
} // namespace org