#include "org/minima/system/commands/command_exception.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {

CommandException::CommandException(const std::string& zException)
    : std::runtime_error(zException) {}

CommandException::CommandException(const char* zException)
    : std::runtime_error(zException ? zException : "") {}

}  // namespace commands
}  // namespace system
}  // namespace minima
}  // namespace org