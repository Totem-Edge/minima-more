#pragma once

#include <string>

// Forward declarations for project types used in signatures (namespaced)
namespace org { namespace minima { namespace kissvm { class Contract; } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {

/**
 * Abstract interface for a KISSVM statement.
 * 
 * Execute the Statement in the given Contract Environment.
 */
class Statement {
public:
    virtual ~Statement();

    // Execute the statement within the provided Contract environment.
    // May throw org::minima::kissvm::exceptions::ExecutionException.
    virtual void execute(org::minima::kissvm::Contract& zContract) = 0;

    // String representation used for tracing/logging.
    // Derived classes should override to provide meaningful output.
    virtual std::string toString() const;
};

} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org