#pragma once

#include <memory>
#include <string>
#include "org/minima/kissvm/statements/statement.hpp"

// Forward declarations for project types used in members/signatures
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

class RETURNstatement final : public org::minima::kissvm::statements::Statement {
public:
    // Constructor takes ownership of the expression
    explicit RETURNstatement(std::unique_ptr<org::minima::kissvm::expressions::Expression> zReturnValue);

    // Destructor and move operations (Pitfall 1: PIMPL fix for unique_ptr to forward-declared type)
    virtual ~RETURNstatement();
    RETURNstatement(RETURNstatement&&) noexcept;
    RETURNstatement& operator=(RETURNstatement&&) noexcept;

    // Delete copy operations
    RETURNstatement(const RETURNstatement&) = delete;
    RETURNstatement& operator=(const RETURNstatement&) = delete;

    // Execute the statement within the provided Contract environment.
    // May throw org::minima::kissvm::exceptions::ExecutionException.
    void execute(org::minima::kissvm::Contract& zContract) override;

    // String representation
    std::string toString() const override;

private:
    std::unique_ptr<org::minima::kissvm::expressions::Expression> mReturnValue;
};

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org