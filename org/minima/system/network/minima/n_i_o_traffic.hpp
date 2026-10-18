#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/mini_format.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

class NIOTraffic {
public:
    NIOTraffic();

    // Thread-safe methods (equivalent to Java 'synchronized')
    void reset();

    long long getStartTime();
    long long getTotalRead();
    long long getTotalWrite();

    org::minima::utils::json::JSONObject getBreakdown();

    void addToTotalRead(long long zAmount);
    void addToTotalWrite(long long zAmount);

    void addReadBytes(const std::string& zFrom, long long zAmount);
    void addWriteBytes(const std::string& zFrom, long long zAmount);

private:
    // Data members
    long long mStartTime;
    long long mTotalRead;
    long long mTotalWrite;

    std::unordered_map<std::string, long long> mReadBreakDown;
    std::unordered_map<std::string, long long> mWriteBreakDown;

    // Mutex to emulate Java's "synchronized" behavior
    std::mutex mMutex;

    // Helper to get current time in milliseconds since epoch
    static long long currentTimeMillis();
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org