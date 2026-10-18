#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "org/minima/system/network/rpc/h_t_t_p_s_server.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

class CMDHandler {
public:
    // Plain HTTP handler.
    explicit CMDHandler(std::intptr_t zSocket);

    // HTTPS handler: receives an SSLClient from HTTPSServer and performs TLS I/O.
    explicit CMDHandler(std::shared_ptr<HTTPSServer::SSLClient> zSSLClient);

    ~CMDHandler();

    // Non-copyable
    CMDHandler(const CMDHandler&) = delete;
    CMDHandler& operator=(const CMDHandler&) = delete;

    // Movable
    CMDHandler(CMDHandler&&) noexcept;
    CMDHandler& operator=(CMDHandler&&) noexcept;

    // Main entry point (equivalent to Java Runnable.run)
    void run();

private:
    std::intptr_t mSocket; // OS socket handle (casted to platform type in .cpp)
    bool mSocketOpen;

    std::shared_ptr<HTTPSServer::SSLClient> mSSLClient;

    void closeSocket() noexcept;
};

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org