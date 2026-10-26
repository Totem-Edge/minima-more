#include "org/minima/system/network/minima/n_i_o_client.hpp"

#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <errno.h>
#endif

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/fast_byte_array_stream.hpp"
#include "org/minima/system/network/minima/connection_backpressure.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"

namespace {

int frameCapForType(std::uint8_t type) {
    switch (type) {
        case 0:  return 1 * 1024 * 1024;       // greeting
        case 1:  return 192 * 1024 * 1024;     // IBD
        case 7:  return 1 * 1024 * 1024;       // P2P
        case 8:  return 1 * 1024 * 1024;       // ping
        case 9:  return 10 * 1024 * 1024;      // lookup
        case 11: return 1 * 1024 * 1024;       // single ping
        case 12: return 1 * 1024 * 1024;       // single pong
        case 13: return 192 * 1024 * 1024;     // IBD request
        case 14: return 192 * 1024 * 1024;     // IBD response
        case 15: return 192 * 1024 * 1024;     // archive request
        case 16: return 192 * 1024 * 1024;     // archive data
        case 17: return 192 * 1024 * 1024;     // archive single request
        case 18: return 64 * 1024 * 1024;      // TxBlockID
        case 19: return 64 * 1024 * 1024;      // TxBlock request
        case 20: return 64 * 1024 * 1024;      // TxBlock
        case 21: return 64 * 1024 * 1024;      // TxBlockMine
        case 22: return 192 * 1024 * 1024;     // MegaMMR request
        case 23: return 192 * 1024 * 1024;     // MegaMMR response
        case 24: return 192 * 1024 * 1024;     // genesis sync request
        case 25: return 192 * 1024 * 1024;     // genesis sync response
        case 2: case 3: case 4: case 5: case 6:
            return 64 * 1024 * 1024;           // TxPoW / pulse / genmessage
        default: return 4 * 1024;              // unknown types
    }
}

} // namespace

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

bool NIOClient::mTraceON = false;

// Helpers to cast the stored portable socket handle to platform types
#ifdef _WIN32
static inline SOCKET to_sock(std::intptr_t s) { return static_cast<SOCKET>(s); }
static inline bool wouldBlock() {
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY;
}
#else
static inline int to_sock(std::intptr_t s) { return static_cast<int>(s); }
static inline bool wouldBlock() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}
#endif

static inline int socket_send_bytes(std::intptr_t s, const std::uint8_t* data, std::size_t len) {
#ifdef _WIN32
    int ret = ::send(to_sock(s), reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
    if (ret == SOCKET_ERROR) {
        if (wouldBlock()) return 0;
        throw std::runtime_error("Socket send error");
    }
    return ret;
#else
    ssize_t ret = ::send(to_sock(s), data, len, 0);
    if (ret < 0) {
        if (wouldBlock()) return 0;
        throw std::runtime_error("Socket send error");
    }
    return static_cast<int>(ret);
#endif
}

static inline int socket_recv_bytes(std::intptr_t s, std::uint8_t* data, std::size_t len) {
#ifdef _WIN32
    int ret = ::recv(to_sock(s), reinterpret_cast<char*>(data), static_cast<int>(len), 0);
    if (ret == 0) {
        // orderly shutdown
        return -1;
    }
    if (ret == SOCKET_ERROR) {
        if (wouldBlock()) return 0;
        throw std::runtime_error("Socket recv error");
    }
    return ret;
#else
    ssize_t ret = ::recv(to_sock(s), data, len, 0);
    if (ret == 0) {
        // orderly shutdown
        return -1;
    }
    if (ret < 0) {
        if (wouldBlock()) return 0;
        throw std::runtime_error("Socket recv error");
    }
    return static_cast<int>(ret);
#endif
}

static inline void socket_close(std::intptr_t s) {
#ifdef _WIN32
    if (s != -1) ::closesocket(to_sock(s));
#else
    if (s >= 0) ::close(to_sock(s));
#endif
}

// Big-endian write/read int32
static inline void putInt32BE(std::vector<std::uint8_t>& buf, std::size_t& pos, std::int32_t v) {
    std::uint8_t b[4];
    b[0] = static_cast<std::uint8_t>((v >> 24) & 0xff);
    b[1] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    b[2] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    b[3] = static_cast<std::uint8_t>(v & 0xff);
    if (pos + 4 > buf.size()) buf.resize(pos + 4);
    std::memcpy(&buf[pos], b, 4);
    pos += 4;
}

static inline std::int32_t getInt32BE(const std::uint8_t* p) {
    return (static_cast<std::int32_t>(p[0]) << 24) |
           (static_cast<std::int32_t>(p[1]) << 16) |
           (static_cast<std::int32_t>(p[2]) << 8)  |
           (static_cast<std::int32_t>(p[3]));
}

// Time helpers
std::int64_t NIOClient::currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string NIOClient::millisToDateString(std::int64_t ms) {
    std::time_t tt = static_cast<std::time_t>(ms / 1000);
    std::tm tmval{};
#ifdef _WIN32
    localtime_s(&tmval, &tt);
#else
    localtime_r(&tt, &tmval);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmval, "%a %b %d %H:%M:%S %Y");
    return oss.str();
}

// Constructors
NIOClient::NIOClient(const std::string& zHost, int zPort)
    : mSocket(-1)
    , mSocketOpen(false)
    , mInBuf()
    , mInSize(0)
    , mOutBuf()
    , mOutSize(0)
    , mReadByteArrayTemp()
    , mHost(zHost)
    , mPort(zPort)
    , mMinimaPort(-1)
    , mIncoming(false)
{
    mUID = org::minima::utils::MiniFormat::createRandomString(8);
    // Buffers allocated on established connection constructor
}

NIOClient::NIOClient(bool zIncoming, const std::string& zHost, int zPort, std::intptr_t zSocket)
    : mSocket(zSocket)
    , mSocketOpen(true)
    , mInBuf()
    , mInSize(0)
    , mOutBuf()
    , mOutSize(0)
    , mReadByteArrayTemp()
    , mHost(zHost)
    , mPort(zPort)
    , mMinimaPort(-1)
    , mIncoming(zIncoming)
{
    mUID = org::minima::utils::MiniFormat::createRandomString(8);

    // Max buffer chunks for read and write
    mInBuf.resize(MAX_NIO_BUFFERS);
    mOutBuf.resize(MAX_NIO_BUFFERS);
    mInSize = 0;
    mOutSize = 0;

    // Create the Read temp array
    mReadByteArrayTemp.resize(MAX_NIO_BUFFERS);

    mTimeConnected   = currentTimeMillis();
    mLastMessageRead = mTimeConnected;
}

// Special members for unique_ptr to forward-declared type
NIOClient::~NIOClient() {
    try {
        disconnect();
    } catch (...) {
        // ignore
    }
}

// Explicit move constructor (do not move mutex)
NIOClient::NIOClient(NIOClient&& other) noexcept
    : mSocket(other.mSocket)
    , mSocketOpen(other.mSocketOpen)
    , mInBuf(std::move(other.mInBuf))
    , mInSize(other.mInSize)
    , mOutBuf(std::move(other.mOutBuf))
    , mOutSize(other.mOutSize)
    , mReadCurrentPosition(other.mReadCurrentPosition)
    , mReadCurrentLimit(other.mReadCurrentLimit)
    , mReadByteArrayTemp(std::move(other.mReadByteArrayTemp))
    , mReadByteArray(std::move(other.mReadByteArray))
    , mWritePosition(other.mWritePosition)
    , mWriteLimit(other.mWriteLimit)
    , mWriteStart(other.mWriteStart)
    , mWriteData(std::move(other.mWriteData))
    , mUID(std::move(other.mUID))
    , mHost(std::move(other.mHost))
    , mPort(other.mPort)
    , mMinimaPort(other.mMinimaPort)
    , mIncoming(other.mIncoming)
    , mMessages(std::move(other.mMessages))
    , mIncomingMsgType(std::move(other.mIncomingMsgType))
    , mPoster(std::move(other.mPoster))
    , mOnReadBytes(std::move(other.mOnReadBytes))
    , mOnWriteBytes(std::move(other.mOnWriteBytes))
    , mWelcomeMessage(std::move(other.mWelcomeMessage))
    , mTimeConnected(other.mTimeConnected)
    , mLastMessageRead(other.mLastMessageRead)
    , mConnectAttempts(other.mConnectAttempts)
    , mValidGreeting(other.mValidGreeting)
    , mSentGreeting(other.mSentGreeting)
    , mP2PGreeting(other.mP2PGreeting)
    , mExtraData(std::move(other.mExtraData))
{
    // Leave mutex default-constructed in this, reset other to safe state
    other.mSocket = -1;
    other.mSocketOpen = false;
    other.mInSize = 0;
    other.mOutSize = 0;
    other.mReadCurrentPosition = 0;
    other.mReadCurrentLimit = 0;
    other.mWritePosition = 0;
    other.mWriteLimit = 0;
    other.mWriteStart = false;
    other.mConnectAttempts = 1;
    other.mValidGreeting = false;
    other.mSentGreeting = false;
    other.mP2PGreeting = false;
    other.mTimeConnected = 0;
    other.mLastMessageRead = 0;
}

// Explicit move assignment (do not move mutex)
NIOClient& NIOClient::operator=(NIOClient&& other) noexcept {
    if (this != &other) {
        // Close our current socket/resources
        disconnect();

        // Move primitive and movable members
        mSocket = other.mSocket;                 other.mSocket = -1;
        mSocketOpen = other.mSocketOpen;         other.mSocketOpen = false;

        mInBuf = std::move(other.mInBuf);
        mInSize = other.mInSize;                 other.mInSize = 0;

        mOutBuf = std::move(other.mOutBuf);
        mOutSize = other.mOutSize;               other.mOutSize = 0;

        mReadCurrentPosition = other.mReadCurrentPosition; other.mReadCurrentPosition = 0;
        mReadCurrentLimit = other.mReadCurrentLimit;       other.mReadCurrentLimit = 0;

        mReadByteArrayTemp = std::move(other.mReadByteArrayTemp);
        mReadByteArray = std::move(other.mReadByteArray);

        mWritePosition = other.mWritePosition;   other.mWritePosition = 0;
        mWriteLimit = other.mWriteLimit;         other.mWriteLimit = 0;
        mWriteStart = other.mWriteStart;         other.mWriteStart = false;
        mWriteData = std::move(other.mWriteData);

        // Move message queue under locks
        {
            std::lock_guard<std::mutex> lock_other(other.mMessagesMutex);
            std::lock_guard<std::mutex> lock_this(mMessagesMutex);
            mMessages = std::move(other.mMessages);
        }

        mUID = std::move(other.mUID);
        mHost = std::move(other.mHost);
        mPort = other.mPort;                     other.mPort = 0;
        mMinimaPort = other.mMinimaPort;         other.mMinimaPort = -1;
        mIncoming = other.mIncoming;             other.mIncoming = false;

        mIncomingMsgType = std::move(other.mIncomingMsgType);
        mPoster = std::move(other.mPoster);
        mOnReadBytes = std::move(other.mOnReadBytes);
        mOnWriteBytes = std::move(other.mOnWriteBytes);

        mWelcomeMessage = std::move(other.mWelcomeMessage);
        mTimeConnected = other.mTimeConnected;   other.mTimeConnected = 0;
        mLastMessageRead = other.mLastMessageRead; other.mLastMessageRead = 0;
        mConnectAttempts = other.mConnectAttempts; other.mConnectAttempts = 1;
        mValidGreeting = other.mValidGreeting;   other.mValidGreeting = false;
        mSentGreeting = other.mSentGreeting;     other.mSentGreeting = false;
        mP2PGreeting = other.mP2PGreeting;       other.mP2PGreeting = false;

        mExtraData = std::move(other.mExtraData);
    }
    return *this;
}

// JSON / string
std::shared_ptr<org::minima::utils::json::JSONObject> NIOClient::toJSON() const {
    auto ret = std::make_shared<org::minima::utils::json::JSONObject>();
    ret->put("welcome", mWelcomeMessage);
    ret->put("uid", mUID);
    ret->put("incoming", mIncoming);
    ret->put("host", mHost);
    ret->put("port", mPort);
    ret->put("minimaport", mMinimaPort);
    ret->put("connected", millisToDateString(mTimeConnected));
    ret->put("valid", mValidGreeting);
    ret->put("sentgreeting", mSentGreeting);
    return ret;
}

std::string NIOClient::toString() const {
    auto j = toJSON();
    return j ? j->toString() : std::string("{}");
}

// Extra data
void NIOClient::setExtraData(const std::any& zExtraData) { mExtraData = zExtraData; }
std::any NIOClient::getExtraData() const { return mExtraData; }

// Simple getters/setters
std::string NIOClient::getUID() const { return mUID; }
bool NIOClient::isIncoming() const { return mIncoming; }
bool NIOClient::isOutgoing() const { return !mIncoming; }
void NIOClient::overrideHost(const std::string& zHost) { mHost = zHost; }
std::string NIOClient::getHost() const { return mHost; }
void NIOClient::setPort(int zPort) { mPort = zPort; }
int NIOClient::getPort() const { return mPort; }
void NIOClient::setMinimaPort(int zPort) { mMinimaPort = zPort; }
int NIOClient::getMinimaPort() const { return mMinimaPort; }
std::string NIOClient::getFullAddress() const {
    if (mMinimaPort != -1) {
        return mHost + ":" + std::to_string(mMinimaPort);
    }
    return mHost + ":" + std::to_string(mPort);
}
std::string NIOClient::getFullMinimaAddress() const { return getFullAddress(); }
bool NIOClient::isValidGreeting() const { return mValidGreeting; }
void NIOClient::setValidGreeting(bool zValid) { mValidGreeting = zValid; }
bool NIOClient::haveSentGreeting() const { return mSentGreeting; }
void NIOClient::setReceivedP2PGreeting() { mP2PGreeting = true; }
bool NIOClient::hasReceivedP2PGreeting() const { return mP2PGreeting; }
void NIOClient::setSentGreeting(bool zSent) { mSentGreeting = zSent; }
std::string NIOClient::getWelcomeMessage() const { return mWelcomeMessage; }
void NIOClient::setWelcomeMessage(const std::string& zWelcome) { mWelcomeMessage = zWelcome; }
std::int64_t NIOClient::getTimeConnected() const { return mTimeConnected; }
std::int64_t NIOClient::getLastReadTime() const { return mLastMessageRead; }
int NIOClient::getConnectAttempts() const { return mConnectAttempts; }
void NIOClient::incrementConnectAttempts() { ++mConnectAttempts; }
void NIOClient::setConnectAttempts(int zConnectAttempts) { mConnectAttempts = zConnectAttempts; }

// Callbacks
void NIOClient::setIncomingMessageType(const std::string& zType) { mIncomingMsgType = zType; }
void NIOClient::setMessagePoster(const std::function<void(org::minima::utils::messages::Message&)>& zPoster) { mPoster = zPoster; }
void NIOClient::setOnReadBytes(const std::function<void(int)>& zOnRead) { mOnReadBytes = zOnRead; }
void NIOClient::setOnWriteBytes(const std::function<void(int)>& zOnWrite) { mOnWriteBytes = zOnWrite; }

// Message queue ops
void NIOClient::sendData(const org::minima::objects::base::MiniData& zData) {
    std::vector<std::uint8_t> bytes = zData.getBytes();
    {
        std::lock_guard<std::mutex> lock(mMessagesMutex);
        mMessages.emplace_back(std::move(bytes));
    }
    // In Java this would set interestOps to OP_WRITE and wake selector; omitted here.
}

bool NIOClient::isNextData() {
    std::lock_guard<std::mutex> lock(mMessagesMutex);
    return !mMessages.empty();
}

std::vector<std::uint8_t> NIOClient::getNextData() {
    std::lock_guard<std::mutex> lock(mMessagesMutex);
    if (!mMessages.empty()) {
        auto bytes = std::move(mMessages.front());
        mMessages.erase(mMessages.begin());
        return bytes;
    }
    return {};
}

// IO
void NIOClient::handleRead() {
    if (!mSocketOpen) {
        throw std::runtime_error("Socket Closed!");
    }

    if (mInBuf.empty()) {
        mInBuf.resize(MAX_NIO_BUFFERS);
    }

    // Read available bytes
    int freeSpace = static_cast<int>(mInBuf.size() - mInSize);
    if (freeSpace <= 0) {
        // No space left: this should not happen with normal usage,
        // but to mimic Java ByteBuffer behavior, treat as no-read event.
        return;
    }

    int readbytes = socket_recv_bytes(mSocket, mInBuf.data() + mInSize, freeSpace);
    if (readbytes == -1) {
        // Socket closed
        throw std::runtime_error("Socket Closed!");
    }

    if (mTraceON) {
        org::minima::utils::MinimaLogger::log("[NIOCLIENT] " + mUID + " read " + std::to_string(readbytes), false);
    }

    if (readbytes == 0) {
        // No data currently available
        return;
    }

    // Traffic listener callback
    if (mOnReadBytes) {
        mOnReadBytes(readbytes);
    }

    mInSize += static_cast<std::size_t>(readbytes);

    // Parse messages from mInBuf[0..mInSize)
    std::size_t offset = 0;
    while (offset < mInSize) {
        if (!mReadByteArray) {
            // Need to read size
            if (mInSize - offset >= 4) {
                mReadCurrentLimit = getInt32BE(mInBuf.data() + offset);
                offset += 4;
                mReadCurrentPosition = 0;

                if (mReadCurrentLimit > MAX_MESSAGE) {
                    throw std::runtime_error("Message too big for read! " + std::to_string(mReadCurrentLimit));
                }

                mReadByteArray = std::make_unique<org::minima::utils::FastByteArrayStream>(mReadCurrentLimit);
            } else {
                // Not enough for size
                break;
            }
        }

        if (mReadByteArray) {
            int readremaining = mReadCurrentLimit - mReadCurrentPosition;
            int buffread = static_cast<int>(mInSize - offset);
            if (mReadCurrentPosition == 0 && buffread > 0) {
                const std::uint8_t type = mInBuf[offset];
                const int cap = frameCapForType(type);
                if (mReadCurrentLimit > cap) {
                    throw std::runtime_error("Frame exceeds per-type cap: type="
                        + std::to_string(type) + " size=" + std::to_string(mReadCurrentLimit)
                        + " cap=" + std::to_string(cap));
                }
            }
            if (buffread > readremaining) buffread = readremaining;
            if (buffread <= 0) break;

            // Copy into temp and write to stream
            if (static_cast<std::size_t>(buffread) > mReadByteArrayTemp.size()) {
                mReadByteArrayTemp.resize(buffread);
            }
            std::memcpy(mReadByteArrayTemp.data(), mInBuf.data() + offset, buffread);
            mReadByteArray->writeData(mReadByteArrayTemp, 0, buffread);

            offset += buffread;
            mReadCurrentPosition += buffread;

            if (mReadCurrentPosition == mReadCurrentLimit) {
                // Complete packet
                std::vector<std::uint8_t> allreaddata = mReadByteArray->toByteArray();


                if (!mIncomingMsgType.empty() && mPoster) {
                    auto& bp = ConnectionBackpressure::getInstance();
                    if (!bp.allowMessage(mUID, mReadCurrentLimit)) {
                        mReadByteArray.reset();
                        throw std::runtime_error("Peer message rate/size limit exceeded");
                    }
                    bp.recordMessage(mUID, mReadCurrentLimit);

                    org::minima::utils::messages::Message msg(mIncomingMsgType);
                    msg.addString("uid", mUID);
                    msg.addString("fullhost", getFullAddress());
                    org::minima::objects::base::MiniData minidata(allreaddata);
                    msg.addObject("data", minidata);
                    mPoster(msg);

                }

                // New array required
                mReadByteArray.reset();

                // Last message we have received from this client
                mLastMessageRead = currentTimeMillis();
            }
        }
    }

    // Compact: move remaining bytes to front
    std::size_t remaining = mInSize - offset;
    if (remaining && offset) {
        std::memmove(mInBuf.data(), mInBuf.data() + offset, remaining);
    }
    mInSize = remaining;
}

void NIOClient::handleWrite() {
    if (!mSocketOpen) {
        throw std::runtime_error("Socket Closed!");
    }

    if (mOutBuf.empty()) {
        mOutBuf.resize(MAX_NIO_BUFFERS);
    }

    // First fill the buffer if it has space
    while (mOutSize < mOutBuf.size()) {
        if (mWriteData.empty()) {
            if (isNextData()) {
                std::vector<std::uint8_t> next = getNextData();
                if (next.empty()) {
                    break;
                }

                if (next.size() > static_cast<std::size_t>(MAX_MESSAGE)) {
                    org::minima::utils::MinimaLogger::log("ERROR : Trying to write a message that is too big! " + std::to_string(next.size()));
                    mWriteData.clear();
                    break;
                }

                mWriteData = std::move(next);
                mWritePosition = 0;
                mWriteLimit = static_cast<int>(mWriteData.size());
                mWriteStart = false;
            } else {
                break;
            }
        }

        if (!mWriteData.empty()) {
            // Write the size
            if (!mWriteStart) {
                if ((mOutBuf.size() - mOutSize) >= 4) {
                    std::size_t pos = mOutSize;
                    putInt32BE(mOutBuf, pos, mWriteLimit);
                    mOutSize = pos;
                    mWriteStart = true;
                } else {
                    break;
                }
            }

            if (mWriteStart) {
                std::size_t remaining = mOutBuf.size() - mOutSize;
                int writeremain = mWriteLimit - mWritePosition;
                if (writeremain <= 0) {
                    // finished
                    mWriteData.clear();
                    continue;
                }

                std::size_t tocopy = static_cast<std::size_t>(writeremain);
                if (tocopy > remaining) {
                    tocopy = remaining;
                }

                if (mOutSize + tocopy > mOutBuf.size()) {
                    // Should not happen since we clamp to remaining
                    break;
                }

                std::memcpy(mOutBuf.data() + mOutSize, mWriteData.data() + mWritePosition, tocopy);
                mOutSize += tocopy;
                mWritePosition += static_cast<int>(tocopy);

                if (mWritePosition == mWriteLimit) {
                    mWriteData.clear();
                }
            }
        }
    }

    // Write out what's in mOutBuf
    int write = 0;
    if (mOutSize > 0) {
        write = socket_send_bytes(mSocket, mOutBuf.data(), mOutSize);
        if (NIOClient::mTraceON) {
            org::minima::utils::MinimaLogger::log("[NIOCLIENT] " + mUID + " wrote : " + std::to_string(write), false);
        }
        if (mOnWriteBytes && write > 0) {
            mOnWriteBytes(write);
        }

        if (write > 0) {

            // Remove written bytes from front (compact)
            std::size_t remaining = mOutSize - static_cast<std::size_t>(write);
            if (remaining > 0) {
                std::memmove(mOutBuf.data(), mOutBuf.data() + write, remaining);
            }
            mOutSize = remaining;
        }
    }

    // InterestOps adjustments are Java-NIO specific; omitted.
}

void NIOClient::disconnect() {
    if (mSocketOpen) {
        try {
            socket_close(mSocket);
        } catch (...) {
            // ignore
        }
        mSocketOpen = false;
        mSocket = -1;
    }
}

std::intptr_t NIOClient::getSocketHandle() const {
    return mSocketOpen ? mSocket : -1;
}

bool NIOClient::hasPendingWrite() const {
    std::lock_guard<std::mutex> lock(mMessagesMutex);
    return !mMessages.empty() || !mWriteData.empty() || mOutSize > 0;
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org