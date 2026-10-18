#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/statements/statement.hpp"

// Forward declarations for project types used in members/signatures
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }
namespace org { namespace minima { namespace kissvm { namespace values { class Value; class BooleanValue; } } } }
namespace org { namespace minima { namespace kissvm { namespace exceptions { class ExecutionException; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

class ASSERTstatement : public org::minima::kissvm::statements::Statement {
public:
    // Constructor takes ownership of the expression
    explicit ASSERTstatement(std::unique_ptr<org::minima::kissvm::expressions::Expression> zAssertValue);

    // Rule 7: Explicitly declare destructor and move operations due to unique_ptr to forward-declared type
    virtual ~ASSERTstatement();
    ASSERTstatement(ASSERTstatement&&) noexcept;
    ASSERTstatement& operator=(ASSERTstatement&&) noexcept;

    // Delete copy operations
    ASSERTstatement(const ASSERTstatement&) = delete;
    ASSERTstatement& operator=(const ASSERTstatement&) = delete;

    // Execute the ASSERT statement logic
    // May throw org::minima::kissvm::exceptions::ExecutionException
    void execute(org::minima::kissvm::Contract& zContract) override;

    // String representation
    std::string toString() const override;

private:
    std::unique_ptr<org::minima::kissvm::expressions::Expression> mAssertValue;
};

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org