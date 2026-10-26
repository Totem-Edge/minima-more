#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <chrono>

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

class ConnectionBackpressure {
public:
    static ConnectionBackpressure& getInstance();

    bool allowMessage(const std::string& clientUID, int messageSize);
    void recordMessage(const std::string& clientUID, int messageSize);
    void removeClient(const std::string& clientUID);

    int getRate(const std::string& clientUID) const;
    void setMaxRate(int messagesPerSecond, int maxBurst);

private:
    ConnectionBackpressure() = default;

    struct ClientStats {
        int messageCount = 0;
        int byteCount = 0;
        std::chrono::steady_clock::time_point windowStart;
        int burstTokens = 0;
    };

    mutable std::mutex mMutex;
    std::unordered_map<std::string, ClientStats> mStats;

    int mMaxMessagesPerSecond = 100;
    int mMaxBurst = 200;
    int mMaxMessageSize = 256 * 1024 * 1024;
    std::chrono::seconds mWindowDuration{1};
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
