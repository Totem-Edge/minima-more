#pragma once

#include <stdexcept>
#include <string>

namespace org {
namespace minima {
namespace system {
namespace commands {

class CommandException : public std::runtime_error {
public:
    explicit CommandException(const std::string& zException);
    explicit CommandException(const char* zException);
    ~CommandException() noexcept override = default;
};

}  // namespace commands
}  // namespace system
}  // namespace minima
}  // namespace org