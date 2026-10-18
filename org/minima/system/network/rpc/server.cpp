#include "org/minima/system/network/rpc/server.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

Server::Server(int zPort)
    : mPort(zPort) {
}

int Server::getPort() const {
    return mPort;
}

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org