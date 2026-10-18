#include "org/minima/system/network/p2p2/p2_p2_manager.hpp"

#include <random>
#include <chrono>
#include <string>

// Project includes
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/timer_message.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/network/p2p2/p2_p2_d_b.hpp"

// The following includes assume typical project paths for these classes.
// Adjust if your project uses different file names/paths.
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"

// OS-specific headers for connectivity check
#ifdef _WIN32
  //
  // FIX 1: Add check to prevent redefinition warning
  //
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p2 {

namespace {

// Cross-platform "is network available" probe: attempt TCP connect to 1.1.1.1:53 with short timeout.
bool isNetworkAvailable() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_pton(AF_INET, "1.1.1.1", &addr.sin_addr);

    int res = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (res == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS && err != WSAEALREADY) {
            closesocket(sock);
            WSACleanup();
            return false;
        }
    }

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sock, &writeSet);
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms

    int sel = select(0, nullptr, &writeSet, nullptr, &tv);
    bool ok = (sel > 0) && FD_ISSET(sock, &writeSet);

    closesocket(sock);
    WSACleanup();
    return ok;
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    if (inet_pton(AF_INET, "1.1.1.1", &addr.sin_addr) <= 0) {
        close(sock);
        return false;
    }

    int res = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (res < 0) {
        if (errno != EINPROGRESS) {
            close(sock);
            return false;
        }
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms

    int sel = select(sock + 1, nullptr, &wfds, nullptr, &tv);
    bool ok = (sel > 0) && FD_ISSET(sock, &wfds);

    close(sock);
    return ok;
#endif
}

} // anonymous namespace

P2P2Manager::P2P2Manager()
    : org::minima::utils::messages::MessageProcessor("P2P2MANAGER") {

    // Do startup: schedule immediate init via 0ms timer
    auto initTimer = std::make_shared<org::minima::utils::messages::TimerMessage>(0, P2P2_INIT);
    PostTimerMessage(initTimer);

    // LOOP checks
    PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(P2P2_LOOP_TIMER, P2P2_FASTLOOP));
    PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(P2P2_LOOP_TIMER_SLOW, P2P2_SLOWLOOP));

    startMessageProcessorThread();
}

void P2P2Manager::shutdown() {
    // Save peers if needed (omitted in Java)
    stopMessageProcessor();
    org::minima::utils::MinimaLogger::log("P2P2 shutdown.. ");
}

std::string P2P2Manager::getRandomPeerFromList() {
    //
    // FIX 2: Get P2P2DB as a reference (auto&)
    //
    auto& pdb = org::minima::database::MinimaDB::getDB()->getP2P2DB();
    //
    // FIX 3: Use dot '.' operator on the 'pdb' reference
    //
    std::vector<std::string>& allpeers = pdb.getAllKnownPeers();

    if (allpeers.empty()) {
        return std::string();
    }

    // Random index selection
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<std::size_t> dist(0, allpeers.size() - 1);
    const std::size_t idx = dist(rng);

    return allpeers[idx];
}

void P2P2Manager::checkWhichConnect() {
    // Not implemented in Java (placeholder for future logic)
}

void P2P2Manager::convertOldP2P() {
    // Currently no-op as in Java code snippet (was commented out in use)
}

void P2P2Manager::processMessage(org::minima::utils::messages::Message& zMessage) {
    using org::minima::utils::MinimaLogger;
    using org::minima::database::MinimaDB;

    const std::string& mtype = zMessage.getMessageType();

    if (mtype == P2P2_INIT) {

        MinimaLogger::log("P2P2 Inited.. ");

        //
        // FIX 3: Use dot '.' operator on the reference returned by getP2P2DB()
        //
        MinimaDB::getDB()->getP2P2DB().addPeerToAllKnown("324.45.45.45:9901");
        MinimaDB::getDB()->getP2P2DB().addPeerToAllKnown("324.45.45.45:9901");
        MinimaDB::getDB()->getP2P2DB().addPeerToAllKnown("324.45.45.45:9902");

        if (MinimaDB::getDB()->getP2P2DB().isFirstStartUp()) {
            // convertOldP2P(); // commented out in Java
            // Potential initial ping to MEGA was commented out in Java
        }

    } else if (mtype == P2P2_FASTLOOP) {

        // Re-schedule
        PostTimerMessage(std::make_shared<org::minima::utils::messages::TimerMessage>(P2P2_LOOP_TIMER, P2P2_FASTLOOP));

        // Check network available
        if (!isNetworkAvailable()) {
            return;
        }

        // See how many connections we have
        int numconn = 0;
        {
            auto* main = org::minima::system::Main::getInstance();
            if (main) {
                numconn = main->getNIOManager().getNumberOfConnectedClients();
            }
        }

        if (numconn < NUMBER_DESIRED_CONNECTIONS) {
            // Connect to a new random peer
            std::string peer = getRandomPeerFromList();

            // Try and connect to him..
            // P2P2Functions.checkConnect(zHost, zPort) // not implemented in Java
            return;
        }

    } else if (mtype == P2P2_SLOWLOOP) {
        // Cycle one of your OUTGOING connections.. close one and connect
        // Connect to a new Host
    }
}

} // namespace p2p2
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
