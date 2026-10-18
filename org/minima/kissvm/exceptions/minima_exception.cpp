#include "org/minima/kissvm/exceptions/minima_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace exceptions {

MinimaException::MinimaException(const std::string& error)
    : m_error(error) {}

const char* MinimaException::what() const noexcept {
    return m_error.c_str();
}

} // namespace exceptions
} // namespace kissvm
} // namespace minima
} // namespace org