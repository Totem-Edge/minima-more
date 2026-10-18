#pragma once

#include <string>
#include <exception>
#include <cstdint>

namespace org {
namespace minima {
namespace utils {

class MinimaLogger {
public:
    static const std::string MINIMA_LOG;

    // Log a message (notify = true)
    static void log(const std::string& zLog);

    // Log a message with explicit notify flag
    static void log(const std::string& zLog, bool zNotify);

    // Log an exception (notify = true)
    static void log(const std::exception& zException);

    // Log an exception with explicit notify flag
    static void log(const std::exception& zException, bool zNotify);

    // Log an uncaught exception with explicit notify flag
    static void logUncaught(const std::exception& zThrow, bool zNotify);

private:
    // Helpers
    static std::string currentDateTime();
    static std::uint64_t getProcessMemoryBytes();
    static std::string formatSize(std::uint64_t bytes);
    static void logExceptionDetails(const std::string& what, bool zNotify);
    static void logStackTracePlatform(bool zNotify);
};

} // namespace utils
} // namespace minima
} // namespace org