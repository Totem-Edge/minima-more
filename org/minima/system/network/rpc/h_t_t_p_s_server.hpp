#pragma once

#include <memory>
#include <functional>
#include <string>

namespace org {
namespace minima {
namespace utils {
class MinimaLogger;
namespace ssl {
class SSLManager;
} // namespace ssl
} // namespace utils
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

class HTTPSServer {
public:
    // TLS-connected client socket wrapper (minimal SSLSocket analogue)
    class SSLClient {
    public:
        ~SSLClient();
        SSLClient(SSLClient&&) noexcept;
        SSLClient& operator=(SSLClient&&) noexcept;

        SSLClient(const SSLClient&) = delete;
        SSLClient& operator=(const SSLClient&) = delete;

        // Read up to len bytes into buf. Returns number of bytes read or negative on error.
        // Blocks until data is available or error occurs.
        long read(void* buf, size_t len);

        // Write len bytes from buf. Returns number of bytes written or negative on error.
        long write(const void* buf, size_t len);

        // Gracefully shutdown and close the connection.
        void close();

        // True if the underlying SSL connection is established and socket is valid.
        bool isValid() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

        // Only HTTPSServer creates SSLClient instances
        SSLClient();
        friend class HTTPSServer;
    };

    // Construct on port. Does NOT start the server thread; call start()
    // explicitly after the most-derived object is fully constructed.
    explicit HTTPSServer(int port);
    virtual ~HTTPSServer();
    HTTPSServer(HTTPSServer&&) noexcept;
    HTTPSServer& operator=(HTTPSServer&&) noexcept;

    HTTPSServer(const HTTPSServer&) = delete;
    HTTPSServer& operator=(const HTTPSServer&) = delete;

    // Start the server loop in a background thread.
    void start();

    // Stop accepting new connections and close the listening socket.
    void shutdown();

    // The configured listening port.
    int getPort() const;

    // Server loop (invoked on internal thread).
    void run();

    // Subclasses must provide a handler factory for each accepted client.
    // The returned function will be executed on a detached thread.
    virtual std::function<void()> getSocketHandler(std::shared_ptr<SSLClient> zSocket) = 0;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org