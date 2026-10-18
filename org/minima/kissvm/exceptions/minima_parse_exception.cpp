#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace exceptions {

MinimaParseException::MinimaParseException(const std::string& zParseError)
    : MinimaException(zParseError) {
}

} // namespace exceptions
} // namespace kissvm
} // namespace minima
} // namespace org