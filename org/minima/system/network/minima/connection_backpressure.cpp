#include "org/minima/system/network/minima/connection_backpressure.hpp"

#include <algorithm>

#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

using org::minima::utils::MinimaLogger;

ConnectionBackpressure& ConnectionBackpressure::getInstance() {
    static ConnectionBackpressure instance;
    return instance;
}

bool ConnectionBackpressure::allowMessage(const std::string& clientUID, int messageSize) {
    if (messageSize > mMaxMessageSize) {
        MinimaLogger::log("[BACKPRESSURE] Message too large from " + clientUID
                          + ": " + std::to_string(messageSize) + " bytes");
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    auto now = std::chrono::steady_clock::now();

    auto& stats = mStats[clientUID];
    if (stats.windowStart == std::chrono::steady_clock::time_point{}) {
        stats.windowStart = now;
        stats.burstTokens = mMaxBurst;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stats.windowStart);
    if (elapsed >= mWindowDuration) {
        stats.messageCount = 0;
        stats.byteCount = 0;
        stats.windowStart = now;
        stats.burstTokens = std::min(mMaxBurst, stats.burstTokens + mMaxMessagesPerSecond);
    }

    if (stats.burstTokens <= 0) {
        return false;
    }

    stats.burstTokens--;
    return true;
}

void ConnectionBackpressure::recordMessage(const std::string& clientUID, int messageSize) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto& stats = mStats[clientUID];
    stats.messageCount++;
    stats.byteCount += messageSize;
}

void ConnectionBackpressure::removeClient(const std::string& clientUID) {
    std::lock_guard<std::mutex> lock(mMutex);
    mStats.erase(clientUID);
}

int ConnectionBackpressure::getRate(const std::string& clientUID) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mStats.find(clientUID);
    if (it != mStats.end()) return it->second.messageCount;
    return 0;
}

void ConnectionBackpressure::setMaxRate(int messagesPerSecond, int maxBurst) {
    mMaxMessagesPerSecond = messagesPerSecond;
    mMaxBurst = maxBurst;
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
