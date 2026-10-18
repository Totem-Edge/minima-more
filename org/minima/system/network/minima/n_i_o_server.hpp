#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <thread>

// Forward declarations for project classes used in members/signatures
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class MessageProcessor; class Message; } } } }
namespace org { namespace minima { namespace system { namespace network { namespace minima { class NIOClient; } } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

// Cross-platform socket handle stored as an integer type in the interface.
using SocketHandle = std::intptr_t;

class NIOServer {
public:
    // Static trace flag (equivalent to Java's public static boolean mTraceON)
    static std::atomic<bool> mTraceON;

    // Construct with port and a MessageProcessor (manager) reference
    NIOServer(int zPort, org::minima::utils::messages::MessageProcessor& zNIOManager);

    // Non-copyable, non-movable (owns server resources and thread-affine sockets)
    NIOServer(const NIOServer&) = delete;
    NIOServer& operator=(const NIOServer&) = delete;
    NIOServer(NIOServer&&) = delete;
    NIOServer& operator=(NIOServer&&) = delete;

    ~NIOServer();

    // Control
    void start();
    void shutdown();

    // Query
    int  getNetClientSize() const;
    bool isRunning() const;

    // Client access
    std::vector<std::shared_ptr<NIOClient>> getAllNIOClients();
    std::shared_ptr<NIOClient> getClient(const std::string& zUID);

    // External socket registration and disconnection
    void regsiterNewSocket(SocketHandle zSocket);
    void disconnect(const std::string& zUID);

    // Messaging to a single client or all clients
    void sendMessage(const std::string& zUID, const org::minima::objects::base::MiniData& zData);
    void sendMessageAll(const org::minima::objects::base::MiniData& zData);

    // Runnable entry point (call on a thread)
    void run();

private:
    // Accepts and configures a new socket, constructs an NIOClient, and announces it
    void addChannel(bool zIncoming, SocketHandle zSocket);

    // Platform helpers
    static void setSocketNonBlocking(SocketHandle sock);
    static void setSocketOptions(SocketHandle sock);
    static void closeSocket(SocketHandle sock);
    static std::pair<std::string, int> getPeerAddressAndPort(SocketHandle sock);

    // Server setup helpers
    SocketHandle createAndBindServer(int port);
    void acceptNewConnections(SocketHandle serverSock);

private:
    // Manager to post messages to
    org::minima::utils::messages::MessageProcessor& mNIOManager;

    int mPort;
    std::atomic<bool> mShutDown;
    std::atomic<bool> mIsRunning;

    // Server socket handle (-1 when not open)
    SocketHandle mServerSocket;

    // All connected clients by UID
    std::unordered_map<std::string, std::shared_ptr<NIOClient>> mClients;

    // Lists protected by mutexes (equivalent to synchronized blocks in Java)
    std::vector<SocketHandle> mRegisterChannels;
    std::vector<std::string>  mDisconnectChannels;
    std::mutex mRegisterMutex;
    std::mutex mDisconnectMutex;
    mutable std::mutex mClientsMutex; // protect mClients for iteration and modification (mutable for const accessors)

    std::thread mServerThread;

    // Self-pipe used to wake select() when outbound data is queued or on shutdown.
    // mWakeupRead is read side, mWakeupWrite is write side.
    SocketHandle mWakeupRead;
    SocketHandle mWakeupWrite;

    // Create the wakeup pipe/socketpair. Returns true on success.
    bool createWakeupPipe();
    // Wake the select loop. Safe to call from any thread.
    void wakeupSelect();
    // Drain any wakeup bytes from the read side.
    void drainWakeup();
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org