#pragma once

#include <memory>
#include <string>

#include "org/minima/kissvm/statements/statement.hpp"

// Namespaced forward declarations (Rule 10)
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }
namespace org { namespace minima { namespace kissvm { namespace statements { class StatementBlock; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

class WHILEstatement : public org::minima::kissvm::statements::Statement {
public:
    // Constructor takes ownership of the condition expression and the code block
    WHILEstatement(std::unique_ptr<org::minima::kissvm::expressions::Expression> zWhileCheck,
                   std::unique_ptr<org::minima::kissvm::statements::StatementBlock> zCodeBlock);

    // Rule 7: explicitly declare destructor and move operations (unique_ptr to incomplete types)
    ~WHILEstatement() override;
    WHILEstatement(WHILEstatement&&) noexcept;
    WHILEstatement& operator=(WHILEstatement&&) noexcept;

    // No copying
    WHILEstatement(const WHILEstatement&) = delete;
    WHILEstatement& operator=(const WHILEstatement&) = delete;

    // Execute the while-statement logic; may throw ExecutionException
    void execute(org::minima::kissvm::Contract& zContract) override;

    // String representation (kept simple to avoid relying on Expression::toString)
    std::string toString() const override;

private:
    std::unique_ptr<org::minima::kissvm::expressions::Expression> mWhileCheck;
    std::unique_ptr<org::minima::kissvm::statements::StatementBlock> mWhileBlock;
};

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org