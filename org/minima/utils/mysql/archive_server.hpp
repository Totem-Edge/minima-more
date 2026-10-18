#pragma once

#include <memory>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

#include "org/minima/system/network/rpc/h_t_t_p_server.hpp"

// Forward declarations (namespaced per Pitfall 4)
namespace org { namespace minima { namespace database { namespace cascade { class Cascade; } } } }
namespace org { namespace minima { namespace objects { class TxBlock; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

// Forward-declare MySQLConnect in the same namespace as Java package
namespace org { namespace minima { namespace utils { namespace mysql {
class MySQLConnect;
} } } }

namespace org {
namespace minima {
namespace utils {
namespace mysql {

class ArchiveServer : public org::minima::system::network::rpc::HTTPServer {
public:
    // Constructor matching Java signature
    ArchiveServer(int zPort,
                  const std::string& zServer,
                  const std::string& zDB,
                  const std::string& zUser,
                  const std::string& zPassword);

    // Destructor required due to unique_ptr to incomplete types (Pitfall 1)
    virtual ~ArchiveServer();

    // HTTPServer base class is non-movable, so derived class cannot be moved either.
    ArchiveServer(ArchiveServer&&) = delete;
    ArchiveServer& operator=(ArchiveServer&&) = delete;

    // Delete copy operations
    ArchiveServer(const ArchiveServer&) = delete;
    ArchiveServer& operator=(const ArchiveServer&) = delete;

    // HTTPServer override: return handler for accepted socket
    std::function<void()> getSocketHandler(org::minima::system::network::rpc::HTTPServer::NativeSocket clientSocket) override;

    // Static clean method (mirrors Java's synchronized SystemClean)
    static void SystemClean();

private:
    // Per-connection handler
    void handleClient(org::minima::system::network::rpc::HTTPServer::NativeSocket clientSocket);

    // Retry helper (Java synchronized method semantics are per-instance)
    std::vector<std::shared_ptr<org::minima::objects::TxBlock>>
    reconnectLoadTxBlocks(const org::minima::objects::base::MiniNumber& zFirstBlock);

private:
    std::unique_ptr<org::minima::utils::mysql::MySQLConnect> mMySQL;
    std::unique_ptr<org::minima::database::cascade::Cascade> mCascade;

    // static last clean timestamp (ms since epoch)
    static std::int64_t mLastClean;
};

} // namespace mysql
} // namespace utils
} // namespace minima
} // namespace org