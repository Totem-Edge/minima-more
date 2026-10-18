#include "org/minima/kissvm/exceptions/execution_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace exceptions {

ExecutionException::ExecutionException(const std::string& error)
    : MinimaException(error) {}

} // namespace exceptions
} // namespace kissvm
} // namespace minima
} // namespace org