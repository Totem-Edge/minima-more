#pragma once

#include <memory>
#include <string>

// Forward declarations for project types used in members/signatures
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; } } } }
namespace org { namespace minima { namespace kissvm { namespace functions { class MinimaFunction; } } } }

// Base class include (inheritance requires full header per rules)
#include "org/minima/kissvm/expressions/expression.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

class FunctionExpression : public Expression {
public:
    explicit FunctionExpression(std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> zFunction);

    // PIMPL-FIX special members due to unique_ptr to forward-declared type
    ~FunctionExpression() override;
    FunctionExpression(FunctionExpression&&) noexcept;
    FunctionExpression& operator=(FunctionExpression&&) noexcept;
    FunctionExpression(const FunctionExpression&) = delete;
    FunctionExpression& operator=(const FunctionExpression&) = delete;

    // Expression interface override (matches base signature)
    org::minima::kissvm::values::Value* getValue(org::minima::kissvm::Contract& zContract) override;

    // String representation (not an override of base)
    std::string toString() const;

private:
    std::unique_ptr<org::minima::kissvm::functions::MinimaFunction> mFunction;
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org