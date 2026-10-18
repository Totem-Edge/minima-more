#pragma once

#include <memory>
#include <vector>
#include <string>

namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace statements { class Statement; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {

class StatementBlock {
public:
    // Take ownership of the statements
    explicit StatementBlock(std::vector<std::unique_ptr<Statement>>&& zStatements);

    // Rule 7: explicitly declare destructor and move operations (unique_ptr to incomplete type)
    ~StatementBlock();
    StatementBlock(StatementBlock&&) noexcept;
    StatementBlock& operator=(StatementBlock&&) noexcept;

    // No copying
    StatementBlock(const StatementBlock&) = delete;
    StatementBlock& operator=(const StatementBlock&) = delete;

    // Run the list of statements; may throw ExecutionException from underlying calls
    void run(org::minima::kissvm::Contract& zContract);

private:
    std::vector<std::unique_ptr<Statement>> mStatements;
};

} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org