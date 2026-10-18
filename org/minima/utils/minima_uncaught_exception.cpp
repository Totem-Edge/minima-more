#include "org/minima/utils/minima_uncaught_exception.hpp"

#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <system_error>
#include <cerrno>
#include <typeinfo>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif
#endif

namespace org {
namespace minima {
namespace utils {

namespace {
// Local logging helpers to avoid dependency on external logger types.
inline void Log(const std::string& msg, bool flush = false) {
    std::fputs((msg + "\n").c_str(), stderr);
    if (flush) {
        std::fflush(stderr);
    }
}

inline void LogUncaught(const std::exception& e, bool flush = false) {
    std::ostringstream oss;
    oss << "[!] EXCEPTION TYPE: " << typeid(e).name() << " WHAT: " << e.what();
    Log(oss.str(), flush);
}

// Cross-platform check for timeout-like std::system_error.
inline bool IsTimeoutError(const std::system_error& e) {
    const std::error_code ec = e.code();

    // Prefer checking against well-known numeric values in appropriate categories.
    const std::error_category& cat = ec.category();
    const int val = ec.value();

    // POSIX: ETIMEDOUT (typically 110)
#ifdef ETIMEDOUT
    if ((&cat == &std::generic_category() || &cat == &std::system_category()) && val == ETIMEDOUT) {
        return true;
    }
#endif

    // Windows winsock timeout: WSAETIMEDOUT (10060) - avoid including winsock headers
#ifdef _WIN32
    if ((&cat == &std::generic_category() || &cat == &std::system_category()) && val == 10060) {
        return true;
    }
#endif

    // Fallback: look for "timed out" substring in the message (case-insensitive)
    try {
        std::string msg = ec.message();
        std::string low = msg;
        std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (low.find("timed out") != std::string::npos || low.find("timeout") != std::string::npos) {
            return true;
        }
    } catch (...) {
        // ignore any exceptions from message retrieval
    }

    return false;
}

} // anonymous namespace

std::terminate_handler MinimaUncaughtException::mDefaultUnCaught = nullptr;

MinimaUncaughtException::MinimaUncaughtException() {
    Install();
}

void MinimaUncaughtException::Install() {
    // Only save the default once. If already installed, do not chain repeatedly.
    if (!mDefaultUnCaught) {
        mDefaultUnCaught = std::get_terminate();
        std::set_terminate(&MinimaUncaughtException::TerminateHandler);
    }
}

void MinimaUncaughtException::ImmediateHalt() {
    // Immediate termination analogous to Java's Runtime.halt(0)
    std::_Exit(0);
}

std::string MinimaUncaughtException::CurrentThreadNameOrId() {
#ifdef _WIN32
    DWORD tid = GetCurrentThreadId();
    return std::to_string(static_cast<unsigned long>(tid));
#elif defined(__APPLE__)
    uint64_t tid = 0;
    if (pthread_threadid_np(nullptr, &tid) == 0) {
        return std::to_string(static_cast<unsigned long long>(tid));
    } else {
        std::ostringstream oss;
        oss << static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(pthread_self()));
        return oss.str();
    }
#elif defined(__linux__)
    // Linux gettid via syscall
    #ifdef SYS_gettid
    pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
    return std::to_string(static_cast<long>(tid));
    #else
    std::ostringstream oss;
    oss << static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(pthread_self()));
    return oss.str();
    #endif
#else
    // Generic POSIX fallback: stringify pthread_self()
    std::ostringstream oss;
    oss << static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(pthread_self()));
    return oss.str();
#endif
}

void MinimaUncaughtException::TerminateHandler() {
    // Log thread info
    const std::string thread_info = CurrentThreadNameOrId();
    Log(std::string("[!] UNCAUGHT EXCEPTION at THREAD ") + thread_info);

    // Inspect current exception if any
    if (auto eptr = std::current_exception()) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::bad_alloc& e) {
            LogUncaught(e, true);
            Log("[!] MEMORY ERROR.. SHUTTING DOWN");
            ImmediateHalt();
            return;
        } catch (const std::system_error& e) {
            LogUncaught(e, true);
            if (IsTimeoutError(e)) {
                Log("[!] Concurrent Timeout Exception.. SHUTTING DOWN");
                ImmediateHalt();
                return;
            }
            // fall through to default handler for other system_error
        } catch (const std::exception& e) {
            LogUncaught(e, true);
            // fall through to default handler
        } catch (...) {
            // Non-std exception: no what()/stacktrace available
            Log("[!] UNCAUGHT NON-STANDARD EXCEPTION", true);
            // fall through to default handler
        }
    } else {
        // No active exception (terminate called directly)
        Log("[!] terminate() called with no active exception", true);
    }

    // Pass to the saved default terminate handler
    Log("[!] PASSING UNCAUGHT ERROR TO DEFAULT HANDLER");
    if (mDefaultUnCaught) {
        mDefaultUnCaught();
    } else {
        // Shouldn't happen; be safe.
        std::abort();
    }
}

void MinimaUncaughtException::uncaughtException(const std::string& thread_name, const std::exception& ex) {
    Log(std::string("[!] UNCAUGHT EXCEPTION at THREAD ") + thread_name);
    LogUncaught(ex, true);

    // Special-case handling analogous to the Java code
    // 1) Memory error
    if (dynamic_cast<const std::bad_alloc*>(&ex) != nullptr) {
        Log("[!] MEMORY ERROR.. SHUTTING DOWN");
        ImmediateHalt();
        return;
    }

    // 2) Timeout exception analogue
    const auto* se = dynamic_cast<const std::system_error*>(&ex);
    if (se && IsTimeoutError(*se)) {
        Log("[!] Concurrent Timeout Exception.. SHUTTING DOWN");
        ImmediateHalt();
        return;
    }

    // For all other exceptions, pass to the default handler.
    Log("[!] PASSING UNCAUGHT ERROR TO DEFAULT HANDLER");
    if (mDefaultUnCaught) {
        mDefaultUnCaught();
    } else {
        std::abort();
    }
}

} // namespace utils
} // namespace minima
} // namespace org