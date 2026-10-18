#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <any>
#include <functional>

namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { class FastByteArrayStream; } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class Message; } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

class NIOClient {
public:
    // Show debug information
    static bool mTraceON;

    // 8K buffer for send and receive
    static constexpr int MAX_NIO_BUFFERS = 8 * 1024;

    // The Maximum size of a single message 256MB
    static constexpr int MAX_MESSAGE = 256 * 1024 * 1024;

    // Outgoing constructor (not yet bound to a socket)
    NIOClient(const std::string& zHost, int zPort);

    // Incoming (or established) constructor with socket handle
    // zSocket is a native socket handle castable to SOCKET on Windows, int on POSIX.
    NIOClient(bool zIncoming, const std::string& zHost, int zPort, std::intptr_t zSocket);

    // PIMPL for unique_ptr member to forward-declared type
    virtual ~NIOClient();
    NIOClient(NIOClient&&) noexcept;
    NIOClient& operator=(NIOClient&&) noexcept;
    NIOClient(const NIOClient&) = delete;
    NIOClient& operator=(const NIOClient&) = delete;

    std::string toString() const;

    // Return a JSONObject with the same content the Java version produced.
    // Returned as shared_ptr due to forward-declaration in header.
    std::shared_ptr<org::minima::utils::json::JSONObject> toJSON() const;

    void setExtraData(const std::any& zExtraData);
    std::any getExtraData() const;

    std::string getUID() const;

    bool isIncoming() const;
    bool isOutgoing() const;

    void overrideHost(const std::string& zHost);
    std::string getHost() const;

    void setPort(int zPort);
    int getPort() const;

    void setMinimaPort(int zPort);
    int getMinimaPort() const;

    std::string getFullAddress() const;
    std::string getFullMinimaAddress() const;

    bool isValidGreeting() const;
    void setValidGreeting(bool zValid);

    bool haveSentGreeting() const;
    void setReceivedP2PGreeting();
    bool hasReceivedP2PGreeting() const;
    void setSentGreeting(bool zSent);

    std::string getWelcomeMessage() const;
    void setWelcomeMessage(const std::string& zWelcome);

    std::int64_t getTimeConnected() const;
    std::int64_t getLastReadTime() const;

    int getConnectAttempts() const;
    void incrementConnectAttempts();
    void setConnectAttempts(int zConnectAttempts);

    // Queue data for sending (thread-safe). Equivalent to Java sendData(MiniData).
    void sendData(const org::minima::objects::base::MiniData& zData);

    // Read available bytes from socket, parse framed messages and post Message via callback.
    // Throws std::runtime_error on socket closure or I/O error.
    void handleRead();

    // Write pending bytes to socket. Throws std::runtime_error on I/O error.
    void handleWrite();

    // Close underlying socket (safe to call multiple times).
    void disconnect();

    // Return the raw socket handle so the NIOServer can include this client in select().
    std::intptr_t getSocketHandle() const;

    // True if there are bytes queued for write.
    bool hasPendingWrite() const;

    // Integration callbacks to preserve functional behavior without direct NIOManager dependency:
    // - incoming message type string (e.g., "NIO_INCOMINGMSG")
    void setIncomingMessageType(const std::string& zType);
    // - poster invoked for each received message that Java would PostMessage to NIOManager
    void setMessagePoster(const std::function<void(org::minima::utils::messages::Message&)>& zPoster);
    // - traffic counters
    void setOnReadBytes(const std::function<void(int)>& zOnRead);
    void setOnWriteBytes(const std::function<void(int)>& zOnWrite);

private:
    // Helper - next queued message bytes
    bool isNextData();
    std::vector<std::uint8_t> getNextData();

    static std::int64_t currentTimeMillis();
    static std::string millisToDateString(std::int64_t ms);

    // Socket handle stored portably; cast in .cpp to platform-specific type
    std::intptr_t mSocket { -1 };
    bool mSocketOpen { false };

    // Inbound buffer (ByteBuffer emulation)
    std::vector<std::uint8_t> mInBuf;
    std::size_t mInSize {0}; // number of valid bytes in mInBuf

    // Outbound buffer
    std::vector<std::uint8_t> mOutBuf;
    std::size_t mOutSize {0}; // number of pending bytes in mOutBuf

    // Read state
    int mReadCurrentPosition {0};
    int mReadCurrentLimit {0};
    std::vector<std::uint8_t> mReadByteArrayTemp;
    std::unique_ptr<org::minima::utils::FastByteArrayStream> mReadByteArray;

    // Write state
    int mWritePosition {0};
    int mWriteLimit {0};
    bool mWriteStart {false};
    std::vector<std::uint8_t> mWriteData; // current packet payload

    std::string mUID;

    std::string mHost;
    int mPort {0};
    int mMinimaPort {-1};
    bool mIncoming {false};

    // Queue of messages to write (as bytes)
    std::vector<std::vector<std::uint8_t>> mMessages;
    mutable std::mutex mMessagesMutex;

    // Optional integration callbacks
    std::string mIncomingMsgType;
    std::function<void(org::minima::utils::messages::Message&)> mPoster;
    std::function<void(int)> mOnReadBytes;
    std::function<void(int)> mOnWriteBytes;

    std::string mWelcomeMessage;
    std::int64_t mTimeConnected {0};
    std::int64_t mLastMessageRead {0};
    int mConnectAttempts {1};
    bool mValidGreeting {false};
    bool mSentGreeting {false};
    bool mP2PGreeting {false};

    std::any mExtraData;
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org