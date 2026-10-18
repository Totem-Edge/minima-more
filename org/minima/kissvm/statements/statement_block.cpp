#include "org/minima/kissvm/statements/statement_block.hpp"

#include <utility>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/statements/statement.hpp"

// No OS-specific behavior required here, but keep placeholder for future divergence.
#ifdef _WIN32
// Windows-specific includes or definitions could go here if needed.
#else
// POSIX-specific includes or definitions could go here if needed.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {

StatementBlock::StatementBlock(std::vector<std::unique_ptr<Statement>>&& zStatements)
    : mStatements(std::move(zStatements)) {}

// Define special members out-of-line after full type is known (PITFALL 1)
StatementBlock::~StatementBlock() = default;
StatementBlock::StatementBlock(StatementBlock&&) noexcept = default;
StatementBlock& StatementBlock::operator=(StatementBlock&&) noexcept = default;

void StatementBlock::run(org::minima::kissvm::Contract& zContract) {
    // Increment Stack Depth
    zContract.incrementStackDepth();

    // Cycle through all the statements
    for (const auto& stat : mStatements) {
        // Check for EXIT
        if (zContract.isSuccessSet()) {
            return; // Note: intentionally no decrement here, matching Java behavior
        }

        // This action counts as one instruction
        zContract.incrementInstructions();

        // Trace it
        zContract.traceLog(stat->toString());

        // Run the next Statement
        stat->execute(zContract);
    }

    // Decrement Stack Depth
    zContract.decrementStackDepth();
}

} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org