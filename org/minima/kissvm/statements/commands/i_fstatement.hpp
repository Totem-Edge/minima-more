#pragma once

#include <memory>
#include <vector>
#include <string>

#include "org/minima/kissvm/statements/statement.hpp"

// Forward declarations for project types used in members/signatures
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }
namespace org { namespace minima { namespace kissvm { namespace statements { class StatementBlock; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {
namespace commands {

class IFstatement : public org::minima::kissvm::statements::Statement {
public:
    IFstatement();

    // Rule 7: explicitly declare destructor and move operations (unique_ptr to incomplete types)
    ~IFstatement() override;
    IFstatement(IFstatement&&) noexcept;
    IFstatement& operator=(IFstatement&&) noexcept;

    // No copying
    IFstatement(const IFstatement&) = delete;
    IFstatement& operator=(const IFstatement&) = delete;

    // Add a condition and its corresponding code block; this class takes ownership
    void addCondition(std::unique_ptr<org::minima::kissvm::expressions::Expression> zCondition,
                      std::unique_ptr<org::minima::kissvm::statements::StatementBlock> zCodeBlock);

    // Execute the full IF/ELSEIF/ELSE chain
    void execute(org::minima::kissvm::Contract& zContract) override;

    // String representation for tracing
    std::string toString() const override;

private:
    // A list of all the conditional statements
    std::vector<std::unique_ptr<org::minima::kissvm::expressions::Expression>> mConditions;

    // A list of the code blocks to be run for each conditional statement
    std::vector<std::unique_ptr<org::minima::kissvm::statements::StatementBlock>> mActions;
};

} // namespace commands
} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org