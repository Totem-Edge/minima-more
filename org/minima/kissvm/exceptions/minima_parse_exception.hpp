#pragma once

#include <string>
#include "org/minima/kissvm/exceptions/minima_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace exceptions {

class MinimaParseException : public MinimaException {
public:
    explicit MinimaParseException(const std::string& zParseError);
    virtual ~MinimaParseException() noexcept = default;
};

} // namespace exceptions
} // namespace kissvm
} // namespace minima
} // namespace org