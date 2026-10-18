#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/statements/statement.hpp"

// Forward declarations (Rule 10)
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

class MASTstatement final : public org::minima::kissvm::statements::Statement {
public:
    explicit MASTstatement(std::unique_ptr<org::minima::kissvm::expressions::Expression> zMAST);

    // Pitfall 1: unique_ptr to forward-declared type requires explicit special members
    ~MASTstatement() override;
    MASTstatement(MASTstatement&&) noexcept;
    MASTstatement& operator=(MASTstatement&&) noexcept;

    // No copying
    MASTstatement(const MASTstatement&) = delete;
    MASTstatement& operator=(const MASTstatement&) = delete;

    // Execute the statement; may throw ExecutionException
    void execute(org::minima::kissvm::Contract& zContract) override;

    // String representation for tracing/logging
    std::string toString() const override;

private:
    std::unique_ptr<org::minima::kissvm::expressions::Expression> mMASTScript;
};

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org