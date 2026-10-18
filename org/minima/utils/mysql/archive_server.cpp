#include "org/minima/utils/mysql/archive_server.hpp"

// Full headers for used types (Rule 10)
#include "org/minima/utils/mysql/my_s_q_l_connect.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/utils/minima_logger.hpp"

#include <vector>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <cerrno>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  static inline void close_socket(SocketHandle s) { if (s != INVALID_SOCKET) ::closesocket(s); }
  static inline int last_socket_error() { return WSAGetLastError(); }
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  using SocketHandle = int;
  static inline void close_socket(SocketHandle s) { if (s >= 0) ::close(s); }
  // last_socket_error is defined but currently unused in this TU; keep for symmetry with _WIN32 branch.
  [[maybe_unused]] static inline int last_socket_error() { return errno; }
#endif

namespace {
    // Read exactly len bytes into buf; return true on success
    bool recv_all(SocketHandle sock, void* buf, size_t len) {
        std::uint8_t* p = static_cast<std::uint8_t*>(buf);
        size_t total = 0;
        while (total < len) {
#ifdef _WIN32
            int r = ::recv(sock, reinterpret_cast<char*>(p + total), static_cast<int>(len - total), 0);
#else
            ssize_t r = ::recv(sock, p + total, len - total, 0);
#endif
            if (r <= 0) {
                return false;
            }
            total += static_cast<size_t>(r);
        }
        return true;
    }

    // Send exactly len bytes from buf; return true on success
    bool send_all(SocketHandle sock, const void* buf, size_t len) {
        const std::uint8_t* p = static_cast<const std::uint8_t*>(buf);
        size_t total = 0;
        while (total < len) {
#ifdef _WIN32
            int s = ::send(sock, reinterpret_cast<const char*>(p + total), static_cast<int>(len - total), 0);
#else
            ssize_t s = ::send(sock, p + total, len - total, 0);
#endif
            if (s <= 0) {
                return false;
            }
            total += static_cast<size_t>(s);
        }
        return true;
    }

    // Read a 32-bit big-endian integer from the socket; return true on success
    bool recv_int32_be(SocketHandle sock, std::int32_t& out) {
        std::uint8_t lenbuf[4];
        if (!recv_all(sock, lenbuf, 4)) return false;
        out = (static_cast<std::int32_t>(lenbuf[0]) << 24) |
              (static_cast<std::int32_t>(lenbuf[1]) << 16) |
              (static_cast<std::int32_t>(lenbuf[2]) << 8)  |
               static_cast<std::int32_t>(lenbuf[3]);
        return true;
    }
}

namespace org {
namespace minima {
namespace utils {
namespace mysql {

// Static member definition
std::int64_t ArchiveServer::mLastClean = 0;

// Destructor (Pitfall 1)
ArchiveServer::~ArchiveServer() = default;

ArchiveServer::ArchiveServer(int zPort,
                             const std::string& zServer,
                             const std::string& zDB,
                             const std::string& zUser,
                             const std::string& zPassword)
: org::minima::system::network::rpc::HTTPServer(zPort, false)
{
    // Initialize DB connection
    mMySQL = std::make_unique<org::minima::utils::mysql::MySQLConnect>(zServer, zDB, zUser, zPassword);
    mMySQL->init();

    // Load cascade (may be null/empty)
    try {
        mCascade = mMySQL->loadCascade();
    } catch (const std::exception& ex) {
        org::minima::utils::MinimaLogger::log(ex);
    }

    if (!mCascade) {
        org::minima::utils::MinimaLogger::log("No Cascade found..");
    } else {
        org::minima::utils::MinimaLogger::log("Cascade found..");
    }

    // Start server
    start();
}

std::function<void()> ArchiveServer::getSocketHandler(org::minima::system::network::rpc::HTTPServer::NativeSocket clientSocket) {
    return [this, clientSocket]() {
        this->handleClient(clientSocket);
    };
}

void ArchiveServer::SystemClean() {
    using namespace std::chrono;
    static std::mutex sCleanMutex;
    std::lock_guard<std::mutex> lock(sCleanMutex);

    auto nowms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    if (nowms - mLastClean > 10000) {
        org::minima::utils::MinimaLogger::log("System Clean..");
        // No explicit GC in C++; rely on RAII
        mLastClean = nowms;
    }
}

std::vector<std::shared_ptr<org::minima::objects::TxBlock>>
ArchiveServer::reconnectLoadTxBlocks(const org::minima::objects::base::MiniNumber& zFirstBlock) {
    
    // Helper lambda to convert the vector type
    auto convert_vector = [](std::vector<std::unique_ptr<org::minima::objects::TxBlock>> unique_vec) {
        std::vector<std::shared_ptr<org::minima::objects::TxBlock>> shared_vec;
        shared_vec.reserve(unique_vec.size());
        for (auto& ptr : unique_vec) {
            // Move ownership from unique_ptr to a new shared_ptr
            shared_vec.push_back(std::move(ptr));
        }
        return shared_vec;
    };

    try {
        // Get the vector of unique_ptrs
        auto unique_blocks = mMySQL->loadBlockRange(zFirstBlock);
        // Convert and return the vector of shared_ptrs
        return convert_vector(std::move(unique_blocks));

    } catch (const std::exception& zExc) {
        org::minima::utils::MinimaLogger::log(std::string("Connection failed.. reconnecting.. : ") + zExc.what());
        // Wait a sec
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Try again
        auto unique_blocks = mMySQL->loadBlockRange(zFirstBlock);
        return convert_vector(std::move(unique_blocks));
    }
}

void ArchiveServer::handleClient(org::minima::system::network::rpc::HTTPServer::NativeSocket clientSocket) {
    SocketHandle sock = static_cast<SocketHandle>(clientSocket);

    try {
        // Read the MiniData frame: 4-byte BE length + payload
        std::int32_t datalen = 0;
        if (!recv_int32_be(sock, datalen)) {
            // Not a valid Minima connection or closed
            close_socket(sock);
            return;
        }

        if (datalen < 0 || datalen > org::minima::objects::base::MiniData::MINIMA_MAX_MINIDATA_LENGTH) {
            // Invalid length
            close_socket(sock);
            return;
        }

        std::vector<std::uint8_t> payload(static_cast<size_t>(datalen));
        if (datalen > 0) {
            if (!recv_all(sock, payload.data(), static_cast<size_t>(datalen))) {
                close_socket(sock);
                return;
            }
        }

        // Parse the payload
        std::string pstr(reinterpret_cast<const char*>(payload.data()), payload.size());
        std::istringstream dis(pstr, std::ios::in | std::ios::binary);

        // What Type..
        org::minima::objects::base::MiniByte type = org::minima::objects::base::MiniByte::ReadFromStream(dis);

        // What block are we starting from..
        org::minima::objects::base::MiniNumber firstblock;
        try {
            firstblock = org::minima::objects::base::MiniNumber::ReadFromStream(dis);
        } catch (const std::exception&) {
            // Not a Minima connection - just random internet traffic
            close_socket(sock);
            return;
        }

        org::minima::utils::MinimaLogger::log(std::string("Received request first block : ") + firstblock.toString(), false);

        // Get the IBD
        org::minima::objects::IBD ibd;

        // Is this the initial
        if (firstblock.isEqual(org::minima::objects::base::MiniNumber::MINUSONE())) {
            // Testing the connection - reconnect to the DB to make sure it is alive..
            (void)reconnectLoadTxBlocks(org::minima::objects::base::MiniNumber::ZERO());
        } else {
            // Do we have a cascade - only check on first call..
            if (firstblock.isEqual(org::minima::objects::base::MiniNumber::ZERO()) && mCascade) {
                org::minima::utils::MinimaLogger::log("Adding cascade..");
                // Deep copy semantics to avoid transferring ownership
                ibd.setCascade(*mCascade);
            }

            // Get the blocks
            auto& ibdblocks = ibd.getTxBlocks();

            // Load the block range..
            auto blocks = reconnectLoadTxBlocks(firstblock);

            // In Java they check for null; here we proceed to append received blocks
            for (auto& blk : blocks) {
                ibdblocks.push_back(blk);
            }
        }

        // Create the network message
        org::minima::objects::base::MiniByte outtype(org::minima::system::network::minima::NIOMessage::MSG_ARCHIVE_DATA());
        auto netdata = org::minima::system::network::minima::NIOManager::createNIOMessage(outtype, ibd);

        // Serialize to buffer and send
        std::ostringstream dos; // binary mode is irrelevant for stringstream
        netdata.writeDataStream(dos);
        const std::string outbuf = dos.str();
        if (!send_all(sock, outbuf.data(), outbuf.size())) {
            // Unable to send; close and exit
            close_socket(sock);
            return;
        }

        // Wait a few seconds for the download to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));

        // Close the socket
        close_socket(sock);
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        close_socket(sock);
    }

    // Clean up
    SystemClean();
}

} // namespace mysql
} // namespace utils
} // namespace minima
} // namespace org

// Port of Java main
#ifdef STANDALONE_TEST
int main(int argc, char* argv[]) {
    using org::minima::utils::MinimaLogger;
    using org::minima::utils::mysql::ArchiveServer;

    MinimaLogger::log("Starting Archive Server v1.3");

    // In Java they load the MySQL driver explicitly; with MariaDB Connector/C this is handled by linker/loader.

    std::string mysqlhost;
    std::string mysqldb;
    std::string mysqluser;
    std::string mysqlpassword;
    int port = 8888;

    // Environment variables
    if (const char* v = std::getenv("MYSQL_HOST"))      mysqlhost = v;
    if (const char* v = std::getenv("MYSQL_DB"))        mysqldb = v;
    if (const char* v = std::getenv("MYSQL_USER"))      mysqluser = v;
    if (const char* v = std::getenv("MYSQL_PASSWORD"))  mysqlpassword = v;
    if (const char* v = std::getenv("ARCHIVE_PORT"))    { try { port = std::stoi(v); } catch (...) {} }

    // Command-line args
    int counter = 0;
    while (counter < argc) {
        std::string arg = argv[counter++];
        if (arg == "-mysqlhost" && counter < argc) {
            mysqlhost = argv[counter++];
        } else if (arg == "-port" && counter < argc) {
            try { port = std::stoi(argv[counter++]); } catch (...) {}
        } else if (arg == "-mysqldb" && counter < argc) {
            mysqldb = argv[counter++];
        } else if (arg == "-mysqluser" && counter < argc) {
            mysqluser = argv[counter++];
        } else if (arg == "-mysqlpassword" && counter < argc) {
            mysqlpassword = argv[counter++];
        } else if (arg == "-help") {
            std::cout << "Minima Archive Server v0.8 Help\n";
            std::cout << " -mysqlhost      : The MySQL Host server\n";
            std::cout << " -mysqldb        : The MySQL Database\n";
            std::cout << " -mysqluser      : The MySQL User\n";
            std::cout << " -mysqlpassword  : The MySQL password\n";
            std::cout << " -help           : Print this help\n";
            return 1;
        } else {
            std::cout << "Unknown parameter : " << arg << "\n";
            return 1;
        }
    }

#ifdef _WIN32
    // Ensure WinSock initialized in executable that uses sockets
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MinimaLogger::log("WSAStartup failed");
        return 1;
    }
#endif

    try {
        ArchiveServer server(port, mysqlhost, mysqldb, mysqluser, mysqlpassword);

        // Listen for input
        std::string input;
        while (true) {
            if (!std::getline(std::cin, input)) {
                break;
            }
            if (!input.empty()) {
                // trim
                auto start = input.find_first_not_of(" \t\r\n");
                auto end   = input.find_last_not_of(" \t\r\n");
                std::string cmd = (start == std::string::npos) ? "" : input.substr(start, end - start + 1);

                if (cmd == "quit") {
                    break;
                } else {
                    MinimaLogger::log(std::string("Unknown command : ") + cmd, false);
                }
            }
        }

        // Stop the Server
        server.shutdown();
        MinimaLogger::log("Archive Server stopped", false);
    } catch (const std::exception& ex) {
        MinimaLogger::log(ex);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
#endif // STANDALONE_TEST