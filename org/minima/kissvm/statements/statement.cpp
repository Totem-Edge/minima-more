#include "org/minima/kissvm/statements/statement.hpp"

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/execution_exception.hpp"

#ifdef _WIN32
// No Windows-specific behavior required for this translation.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {

Statement::~Statement() = default;

std::string Statement::toString() const {
    return "Statement";
}

} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org