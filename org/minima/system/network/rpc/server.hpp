#pragma once

#include <cstdint>

namespace org {
namespace minima {
namespace system {
namespace network {
namespace rpc {

class Server {
protected:
    int mPort = 0;

public:
    explicit Server(int zPort);
    virtual ~Server() = default;

    int getPort() const;

    virtual void shutdown() = 0;
};

} // namespace rpc
} // namespace network
} // namespace system
} // namespace minima
} // namespace org