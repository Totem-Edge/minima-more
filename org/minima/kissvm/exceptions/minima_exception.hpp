#pragma once

#include <exception>
#include <string>

namespace org {
namespace minima {
namespace kissvm {
namespace exceptions {

class MinimaException : public std::exception {
public:
    explicit MinimaException(const std::string& error);
    const char* what() const noexcept override;

private:
    std::string m_error;
};

} // namespace exceptions
} // namespace kissvm
} // namespace minima
} // namespace org