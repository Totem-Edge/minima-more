#include "org/minima/utils/minima_logger.hpp"


#include <iostream>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <fstream>
#include <string>

#ifdef _WIN32
  #include <windows.h>
  #include <psapi.h>
#elif defined(__APPLE__)
  #include <mach/mach.h>
  #include <unistd.h>
#else
  #include <unistd.h>
#endif

#if (defined(__APPLE__) || (defined(__linux__) && defined(__GLIBC__))) && !defined(__ANDROID__)
  #include <execinfo.h>
  #include <cstdlib>
#endif

#include "org/minima/system/main.hpp"

// Minimal forward declarations to avoid including main.hpp (which triggers a base-class error)
// Global forward-declared JSONObject (as used by Main::PostNotifyEvent signatures)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }


// Project JSON header for the real namespaced JSONObject
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Note on Pitfall 3 (Global vs Namespaced JSONObject):
// We bridge org::minima::utils::json::JSONObject to the global forward-declared ::JSONObject
// via reinterpret_cast when calling Main::PostNotifyEvent.

namespace org {
namespace minima {
namespace utils {

const std::string MinimaLogger::MINIMA_LOG = "MINIMALOG";

void MinimaLogger::log(const std::string& zLog) {
    log(zLog, true);
}

namespace {
bool s_useStderr = false;
}

void MinimaLogger::setUseStderr(bool on) {
    s_useStderr = on;
}

void MinimaLogger::log(const std::string& zLog, bool zNotify) {
    // Memory usage approximation by platform
    std::uint64_t memBytes = getProcessMemoryBytes();

    // Format timestamp
    std::string datetime = currentDateTime();

    // Format memory size
    std::string memStr = formatSize(memBytes);

    // Construct full log line
    std::string full_log = std::string("Minima @ ") + datetime + " [" + memStr + "] : " + zLog;

    // Print to stdout (or stderr in `-mcp stdio` mode). Keep the per-line flush
    // (std::endl): the node is normally run with stdout redirected to a
    // file/pipe (fully buffered), and operators rely on log lines appearing
    // promptly. The expensive part of logging was the per-line memory syscall,
    // which is now cached - not the flush.
    if (s_useStderr) std::cerr << full_log << std::endl;
    else std::cout << full_log << std::endl;

    // Notify listeners if required
    if (zNotify) {
        org::minima::system::Main* mainInst = org::minima::system::Main::getInstance();
        if (mainInst != nullptr) {
            org::minima::utils::json::JSONObject data;
            data.put("message", full_log);

            // Bridge namespaced JSONObject to global forward-declared JSONObject
            mainInst->PostNotifyEvent(MINIMA_LOG, data);
        }
    }
}

void MinimaLogger::log(const std::exception& zException) {
    log(zException, true);
}

void MinimaLogger::log(const std::exception& zException, bool zNotify) {
    // First log the exception summary (analogous to Java's toString())
    logExceptionDetails(zException.what(), zNotify);
    // Then best-effort stack trace (where available)
    logStackTracePlatform(zNotify);
}

void MinimaLogger::logUncaught(const std::exception& zThrow, bool zNotify) {
    // First the full exception
    std::string header = std::string("[!] UNCAUGHT EXCEPTION : ") + zThrow.what();
    log(header, zNotify);
    // Then best-effort stack trace
    logStackTracePlatform(zNotify);
}

/* Private helpers */

std::string MinimaLogger::currentDateTime() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t tt = system_clock::to_time_t(now);

    std::tm tm_snapshot{};
#ifdef _WIN32
    localtime_s(&tm_snapshot, &tt);
#else
    localtime_r(&tt, &tm_snapshot);
#endif

    std::ostringstream oss;
    // Java pattern "dd/MM/yyyy HH:mm:ss"
    oss << std::put_time(&tm_snapshot, "%d/%m/%Y %H:%M:%S");
    return oss.str();
}

namespace {
// Raw, uncached resident-set query (platform specific).
std::uint64_t readProcessMemoryBytesUncached() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<std::uint64_t>(pmc.WorkingSetSize);
    }
    return 0;
#elif defined(__APPLE__)
    task_basic_info tinfo;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO,
                                 reinterpret_cast<task_info_t>(&tinfo), &count);
    if (kr == KERN_SUCCESS) {
        return static_cast<std::uint64_t>(tinfo.resident_size);
    }
    return 0;
#else
    // Linux/Unix: read /proc/self/statm for resident set size in pages
    std::ifstream statm("/proc/self/statm");
    if (statm.good()) {
        std::uint64_t totalPages = 0, residentPages = 0;
        statm >> totalPages >> residentPages;
        long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize > 0) {
            return residentPages * static_cast<std::uint64_t>(pageSize);
        }
    }
    return 0;
#endif
}
} // namespace

std::uint64_t MinimaLogger::getProcessMemoryBytes() {
    // Logging can fire hundreds of times per second. Reading /proc (or task_info)
    // on every single line is a needless syscall/alloc storm, and a ~1s-stale
    // figure is perfectly adequate for a log line. Cache it behind a 1s TTL.
    static std::atomic<std::uint64_t> sCachedBytes{0};
    static std::atomic<std::int64_t> sCachedAtMs{0};
    const std::int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const std::int64_t lastMs = sCachedAtMs.load(std::memory_order_relaxed);
    if (lastMs != 0 && (nowMs - lastMs) < 1000) {
        return sCachedBytes.load(std::memory_order_relaxed);
    }
    const std::uint64_t bytes = readProcessMemoryBytesUncached();
    sCachedBytes.store(bytes, std::memory_order_relaxed);
    sCachedAtMs.store(nowMs, std::memory_order_relaxed);
    return bytes;
}

std::string MinimaLogger::formatSize(std::uint64_t bytes) {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double size = static_cast<double>(bytes);
    int idx = 0;
    while (size >= 1024.0 && idx < 5) {
        size /= 1024.0;
        ++idx;
    }
    std::ostringstream oss;
    if (idx == 0) {
        oss << static_cast<std::uint64_t>(size) << " " << suffixes[idx];
    } else {
        oss << std::fixed << std::setprecision(size < 10 ? 2 : 1) << size << " " << suffixes[idx];
    }
    return oss.str();
}

void MinimaLogger::logExceptionDetails(const std::string& what, bool zNotify) {
    // Mimic Java's first line: exception.toString() analogue.
    // We only have what() reliably across platforms.
    log(what, zNotify);
}

void MinimaLogger::logStackTracePlatform(bool zNotify) {
#if (defined(__APPLE__) || (defined(__linux__) && defined(__GLIBC__))) && !defined(__ANDROID__)
    // POSIX best-effort stack trace using backtrace
    void* buffer[64];
    int nptrs = backtrace(buffer, 64);
    if (nptrs > 0) {
        char** symbols = backtrace_symbols(buffer, nptrs);
        if (symbols) {
            for (int i = 0; i < nptrs; ++i) {
                std::string line = std::string("     ") + symbols[i];
                log(line, zNotify);
            }
            free(symbols);
        }
    }
#else
    // On Windows (without extra dependencies), no portable stack trace.
    (void)zNotify;
#endif
}

} // namespace utils
} // namespace minima
} // namespace org