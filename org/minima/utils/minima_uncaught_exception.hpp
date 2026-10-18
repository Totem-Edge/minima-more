#pragma once

#include <exception>
#include <string>

namespace org {
namespace minima {
namespace utils {

class MinimaUncaughtException {
public:
    MinimaUncaughtException();
    MinimaUncaughtException(const MinimaUncaughtException&) = delete;
    MinimaUncaughtException& operator=(const MinimaUncaughtException&) = delete;

    // Emulates Java's uncaughtException(Thread, Throwable)
    // Provide a thread "name" or identifier string and the exception.
    void uncaughtException(const std::string& thread_name, const std::exception& ex);

    // Install this handler as the process-wide terminate handler.
    static void Install();

private:
    static std::terminate_handler mDefaultUnCaught;

    static void TerminateHandler();
    static void ImmediateHalt();
    static std::string CurrentThreadNameOrId();
};

} // namespace utils
} // namespace minima
} // namespace org