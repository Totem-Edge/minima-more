#include "org/minima/system/network/minima/n_i_o_traffic.hpp"

#include <chrono>
#include <any>

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

using org::minima::utils::MiniFormat;
using org::minima::utils::json::JSONObject;

NIOTraffic::NIOTraffic() {
    reset();
}

long long NIOTraffic::currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void NIOTraffic::reset() {
    std::lock_guard<std::mutex> lock(mMutex);

    mStartTime   = currentTimeMillis();
    mTotalRead   = 0;
    mTotalWrite  = 0;

    mReadBreakDown.clear();
    mWriteBreakDown.clear();
}

long long NIOTraffic::getStartTime() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mStartTime;
}

long long NIOTraffic::getTotalRead() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mTotalRead;
}

long long NIOTraffic::getTotalWrite() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mTotalWrite;
}

JSONObject NIOTraffic::getBreakdown() {
    std::lock_guard<std::mutex> lock(mMutex);

    JSONObject ret;

    // Reads
    JSONObject rr;
    for (const auto& kv : mReadBreakDown) {
        const std::string& key = kv.first;
        long long value = kv.second;
        std::string vs = MiniFormat::formatSize(value);
        rr.put(key, vs);
    }

    // Writes
    JSONObject ww;
    for (const auto& kv : mWriteBreakDown) {
        const std::string& key = kv.first;
        long long value = kv.second;
        std::string vs = MiniFormat::formatSize(value);
        ww.put(key, vs);
    }

    // Add to main JSON
    ret.put("reads", rr);
    ret.put("writes", ww);

    return ret;
}

void NIOTraffic::addToTotalRead(long long zAmount) {
    std::lock_guard<std::mutex> lock(mMutex);
    mTotalRead += zAmount;
}

void NIOTraffic::addToTotalWrite(long long zAmount) {
    std::lock_guard<std::mutex> lock(mMutex);
    mTotalWrite += zAmount;
}

void NIOTraffic::addReadBytes(const std::string& zFrom, long long zAmount) {
    std::lock_guard<std::mutex> lock(mMutex);

    long long current = 0;
    auto it = mReadBreakDown.find(zFrom);
    if (it != mReadBreakDown.end()) {
        current = it->second;
    }

    mReadBreakDown[zFrom] = current + zAmount;
}

void NIOTraffic::addWriteBytes(const std::string& zFrom, long long zAmount) {
    std::lock_guard<std::mutex> lock(mMutex);

    long long current = 0;
    auto it = mWriteBreakDown.find(zFrom);
    if (it != mWriteBreakDown.end()) {
        current = it->second;
    }

    mWriteBreakDown[zFrom] = current + zAmount;
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org