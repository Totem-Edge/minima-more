#pragma once

#include <string>
#include "org/minima/kissvm/exceptions/minima_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace exceptions {

class ExecutionException : public MinimaException {
public:
    explicit ExecutionException(const std::string& error);
    ~ExecutionException() noexcept = default;
};

} // namespace exceptions
} // namespace kissvm
} // namespace minima
} // namespace org