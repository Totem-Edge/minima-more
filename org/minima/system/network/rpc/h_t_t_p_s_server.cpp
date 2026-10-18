#include "org/minima/system/network/rpc/h_t_t_p_s_server.hpp"

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/ssl/s_s_l_manager.hpp"

#include <atomic>
#include <thread>
#include <vector>
#include <system_error>
#include <cstring>
#include <cerrno>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <netdb.h>
#endif

// OpenSSL
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

namespace {
#ifdef _WIN32
using socket_handle_t = SOCKET;
constexpr socket_handle_t INVALID_SOCKET_HANDLE = INVALID_SOCKET;
inline void close_socket(socket_handle_t s) {
    if (s != INVALID_SOCKET) {
        ::closesocket(s);
    }
}
inline int last_socket_error() {
    return WSAGetLastError();
}
#else
using socket_handle_t = int;
constexpr socket_handle_t INVALID_SOCKET_HANDLE = -1;
inline void close_socket(socket_handle_t s) {
    if (s >= 0) {
        ::close(s);
    }
}
inline int last_socket_error() {
    return errno;
}
#endif

inline bool is_addr_in_use_error(int err) {
#ifdef _WIN32
    return err == WSAEADDRINUSE;
#else
    return err == EADDRINUSE;
#endif
}

} // anonymous namespace

// ==================== SSLClient::Impl ====================
struct HTTPSServer::SSLClient::Impl {
    socket_handle_t sock{INVALID_SOCKET_HANDLE};
    SSL* ssl{nullptr};
    bool open{false};

    Impl() = default;

    ~Impl() {
        // Ensure cleanup: SSL_free() will also close the underlying BIO/socket.
        if (ssl) {
            // Attempt a graceful shutdown
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }
        // sock is managed by the SSL BIO; avoid double-close.
        sock = INVALID_SOCKET_HANDLE;
        open = false;
    }

    bool valid() const {
        return open && ssl != nullptr;
    }
};

// ==================== HTTPSServer::Impl ====================
struct HTTPSServer::Impl {
    int port{0};
    std::atomic<bool> shutdown{false};
    socket_handle_t listenSock{INVALID_SOCKET_HANDLE};
    std::thread thread;

#ifdef _WIN32
    bool wsaInit{false};
#endif

    Impl() = default;

    ~Impl() {
        // Ensure listening socket is closed
        if (listenSock != INVALID_SOCKET_HANDLE) {
            close_socket(listenSock);
            listenSock = INVALID_SOCKET_HANDLE;
        }
        if (thread.joinable()) {
            thread.join();
        }
#ifdef _WIN32
        if (wsaInit) {
            WSACleanup();
            wsaInit = false;
        }
#endif
    }
};

// ==================== SSLClient methods ====================
HTTPSServer::SSLClient::SSLClient() : m_impl(std::make_unique<Impl>()) {}

HTTPSServer::SSLClient::~SSLClient() = default;
HTTPSServer::SSLClient::SSLClient(SSLClient&&) noexcept = default;
HTTPSServer::SSLClient& HTTPSServer::SSLClient::operator=(SSLClient&&) noexcept = default;

long HTTPSServer::SSLClient::read(void* buf, size_t len) {
    if (!m_impl || !m_impl->valid()) return -1;
    int ret = ::SSL_read(m_impl->ssl, buf, static_cast<int>(len));
    if (ret <= 0) {
        int err = SSL_get_error(m_impl->ssl, ret);
        (void)err; // caller can treat <=0 as error/EOF
    }
    return ret;
}

long HTTPSServer::SSLClient::write(const void* buf, size_t len) {
    if (!m_impl || !m_impl->valid()) return -1;
    int ret = ::SSL_write(m_impl->ssl, buf, static_cast<int>(len));
    if (ret <= 0) {
        int err = SSL_get_error(m_impl->ssl, ret);
        (void)err;
    }
    return ret;
}

void HTTPSServer::SSLClient::close() {
    if (!m_impl) return;
    if (m_impl->ssl) {
        SSL_shutdown(m_impl->ssl);
        SSL_free(m_impl->ssl);
        m_impl->ssl = nullptr;
    }
    // BIO owns the socket; SSL_free already closed it.
    m_impl->sock = INVALID_SOCKET_HANDLE;
    m_impl->open = false;
}

bool HTTPSServer::SSLClient::isValid() const {
    return m_impl && m_impl->valid();
}

// ==================== HTTPSServer methods ====================
HTTPSServer::HTTPSServer(int port) : m_impl(std::make_unique<Impl>()) {
    m_impl->port = port;
    // Do NOT auto-start here. The owner must call start() after the most-derived
    // object is fully constructed, otherwise run() may log / notify while the
    // derived object or NotifyManager is still under construction.
}

HTTPSServer::~HTTPSServer() = default;
HTTPSServer::HTTPSServer(HTTPSServer&&) noexcept = default;
HTTPSServer& HTTPSServer::operator=(HTTPSServer&&) noexcept = default;

void HTTPSServer::start() {
    if (!m_impl) return;
    if (m_impl->thread.joinable()) {
        org::minima::utils::MinimaLogger::log("HTTPSServer already started");
        return;
    }
    m_impl->thread = std::thread(&HTTPSServer::run, this);
}

void HTTPSServer::shutdown() {
    if (!m_impl) return;
    m_impl->shutdown = true;

    // Close listening socket to unblock accept()
    if (m_impl->listenSock != INVALID_SOCKET_HANDLE) {
        close_socket(m_impl->listenSock);
        m_impl->listenSock = INVALID_SOCKET_HANDLE;
    }
}

int HTTPSServer::getPort() const {
    return m_impl ? m_impl->port : 0;
}

void HTTPSServer::run() {
    using org::minima::utils::MinimaLogger;
    using org::minima::utils::ssl::SSLManager;

#ifdef _WIN32
    // Initialize Winsock
    {
        WSADATA wsaData;
        int wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (wsaerr != 0) {
            MinimaLogger::log(std::string("WSAStartup failed: ") + std::to_string(wsaerr));
            return;
        }
        m_impl->wsaInit = true;
    }
#endif

    // Load KeyStore and create SSL_CTX via SSLManager
    std::unique_ptr<SSLManager::LoadedKeyStore> keyStore = SSLManager::getSSLKeyStore();
    if (!keyStore || !keyStore->isValid()) {
        MinimaLogger::log("Failed to load SSL keystore.");
        return;
    }

    std::unique_ptr<SSLManager::KeyManagerFactory> kmf = SSLManager::getSSLKeyFactory(*keyStore);
    if (!kmf) {
        MinimaLogger::log("Failed to create SSL key manager factory.");
        return;
    }

    // Native SSL_CTX* (opaque in header, concrete here)
    SSL_CTX* ssl_ctx = reinterpret_cast<SSL_CTX*>(kmf->nativeHandle());
    if (!ssl_ctx) {
        MinimaLogger::log("SSL_CTX not available from KeyManagerFactory.");
        return;
    }

    // Create and bind listening socket
    socket_handle_t listenSock = INVALID_SOCKET_HANDLE;
    int port = m_impl->port;

    listenSock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET_HANDLE) {
        MinimaLogger::log("Failed to create socket.");
        return;
    }

    // SO_REUSEADDR
    {
        int reuse = 1;
        ::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
            reinterpret_cast<const char*>(&reuse),
#else
            &reuse,
#endif
            sizeof(reuse));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        int err = last_socket_error();
        close_socket(listenSock);
        listenSock = INVALID_SOCKET_HANDLE;

        if (is_addr_in_use_error(err)) {
            MinimaLogger::log("SSL Server @ Port " + std::to_string(port) + " already in use!.. restart required..");
        } else {
            if (!m_impl->shutdown.load()) {
                MinimaLogger::log(std::string("Bind failed: error ") + std::to_string(err));
            }
        }
        return;
    }

    if (::listen(listenSock, SOMAXCONN) != 0) {
        int err = last_socket_error();
        close_socket(listenSock);
        listenSock = INVALID_SOCKET_HANDLE;

        if (!m_impl->shutdown.load()) {
            MinimaLogger::log(std::string("Listen failed: error ") + std::to_string(err));
        }
        return;
    }

    m_impl->listenSock = listenSock;

    // Log server started
    MinimaLogger::log("SSL server started on port " + std::to_string(port));

    // Accept loop
    while (!m_impl->shutdown.load()) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clen = sizeof(clientAddr);
#else
        socklen_t clen = sizeof(clientAddr);
#endif
        socket_handle_t clientSock = ::accept(m_impl->listenSock,
                                              reinterpret_cast<sockaddr*>(&clientAddr),
                                              &clen);
        if (clientSock == INVALID_SOCKET_HANDLE) {
            // If shutting down, accept will fail - exit loop
            if (m_impl->shutdown.load()) {
                break;
            }
            // Transient error - continue
            continue;
        }

        // Create SSL and perform handshake
        SSL* ssl = ::SSL_new(ssl_ctx);
        if (!ssl) {
            close_socket(clientSock);
            continue;
        }

        // Attach the socket to SSL via a BIO so the raw OS handle is never exposed
        // to OpenSSL, avoiding handle-size issues (especially Win64 SOCKET vs int).
        BIO* bio = BIO_new_socket(static_cast<int>(clientSock), BIO_NOCLOSE);
        if (!bio) {
            SSL_free(ssl);
            close_socket(clientSock);
            continue;
        }
        SSL_set_bio(ssl, bio, bio);

        // Wrap in SSLClient before the handshake so the SSLClient destructor owns cleanup.
        auto client = std::shared_ptr<SSLClient>(new SSLClient());
        client->m_impl->ssl  = ssl;
        client->m_impl->open = true;

        int acc = ::SSL_accept(ssl);
        if (acc != 1) {
            // Handshake failed - SSLClient::close()/destructor will free SSL + socket
            continue;
        }

        // Get handler from subclass and run on new detached thread
        try {
            std::function<void()> handler = getSocketHandler(client);
            std::thread t([handler = std::move(handler)]() mutable {
                handler();
            });
            // Daemon equivalent
            t.detach();
        } catch (const std::exception& ex) {
            MinimaLogger::log(ex);
            // Cleanup on failure to spawn handler
            client->close();
        } catch (...) {
            // Unknown exception
            MinimaLogger::log(std::string("Unknown exception creating handler."));
            client->close();
        }
    }

    // Cleanup listening socket
    if (m_impl->listenSock != INVALID_SOCKET_HANDLE) {
        close_socket(m_impl->listenSock);
        m_impl->listenSock = INVALID_SOCKET_HANDLE;
    }
}

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org