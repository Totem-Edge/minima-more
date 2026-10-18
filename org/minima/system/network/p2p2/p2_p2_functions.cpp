#include "org/minima/system/network/p2p2/p2_p2_functions.hpp"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <utility>
#include <stdexcept>
#include <cstdlib>

// Project headers needed (safe ones)
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/message_processor.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"

// Minimal forward declarations to avoid including problematic headers
namespace org { namespace minima { namespace system {
class Main;
} } }

namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOManager;
} } } } }

// Declare only the methods we need (signatures must match real definitions)
namespace org { namespace minima { namespace system {
class Main {
public:
    static Main* getInstance();
    org::minima::system::network::minima::NIOManager* getNIOManager();
};
} } }

namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOManager {
public:
    void disconnect(const std::string& zClientUID);
    std::vector<org::minima::system::network::minima::NIOClientInfo*> getAllConnectionInfo();
};
} } } } }

// Platform networking includes
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <iphlpapi.h>
  #pragma comment(lib, "Ws2_32.lib")
  #pragma comment(lib, "Iphlpapi.lib")
// Prevent Windows macro collision with PostMessage
  #ifdef PostMessage
  #undef PostMessage
  #endif
#else
  #include <ifaddrs.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p2 {

// Static constants
const std::string P2P2Functions::P2P_INIT        = "P2P_INIT";
const std::string P2P2Functions::P2P_SHUTDOWN    = "P2P_SHUTDOWN";
const std::string P2P2Functions::P2P_CONNECTED   = "P2P_CONNECTED";
const std::string P2P2Functions::P2P_DISCONNECTED= "P2P_DISCONNECTED";
const std::string P2P2Functions::P2P_NOCONNECT   = "P2P_NOCONNECT";
const std::string P2P2Functions::P2P_MESSAGE     = "P2P_MESSAGE";

// Static state
std::unordered_set<std::string> P2P2Functions::sInvalidPeers;
bool P2P2Functions::sLocalAddressesInit = false;
std::unordered_set<std::string> P2P2Functions::sLocalAddresses;

static inline bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin());
}

const std::unordered_set<std::string>& P2P2Functions::getLocalAddresses() {
    if (!sLocalAddressesInit) {
        try {
            sLocalAddresses = getAllNetworkInterfaceAddresses();
            sLocalAddressesInit = true;
        } catch (...) {
            // Fallback on failure, mirror Java behavior
            sLocalAddresses.clear();
            sLocalAddresses.insert("localhost");
            sLocalAddresses.insert("127.0.0.1");
            sLocalAddresses.insert("127.0.1.1");
            sLocalAddressesInit = true;
        }
    }
    return sLocalAddresses;
}

void P2P2Functions::addInvalidPeer(const std::string& zHostPost) {
    if (sInvalidPeers.find(zHostPost) == sInvalidPeers.end()) {
        org::minima::utils::MinimaLogger::log("INVALID PEER added to List! " + zHostPost);
        sInvalidPeers.insert(zHostPost);
    } else {
        // Already added; silently ignore
    }
}

bool P2P2Functions::isInvalidPeer(const std::string& zHostPost) {
    return sInvalidPeers.find(zHostPost) != sInvalidPeers.end();
}

void P2P2Functions::clearInvalidPeers() {
    sInvalidPeers.clear();
}

bool P2P2Functions::isIPv6(const std::string& fullhost) {
    auto first = fullhost.find(':');
    auto last  = fullhost.rfind(':');
    return (first != std::string::npos) && (last != std::string::npos) && (first != last);
}

bool P2P2Functions::isIPLocal(const std::string& fullhost) {
    return  starts_with(fullhost, "127.")      ||
            starts_with(fullhost, "localhost") ||
            starts_with(fullhost, "10.")       ||
            starts_with(fullhost, "100.")      ||
            starts_with(fullhost, "0.")        ||
            starts_with(fullhost, "169.")      ||
            starts_with(fullhost, "172.")      ||
            starts_with(fullhost, "198.")      ||
            starts_with(fullhost, "192.");
}

#ifdef _WIN32
static bool winsock_init_once() {
    static bool inited = false;
    static bool ok = false;
    if (!inited) {
        WSADATA wsaData;
        ok = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
        inited = true;
    }
    return ok;
}
#endif

bool P2P2Functions::isNetAvailable() {
    // Try to connect to www.google.com:443 with short timeout
    const char* host = "www.google.com";
    const char* port = "443";

#ifdef _WIN32
    if (!winsock_init_once()) {
        return false;
    }
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) {
        return false;
    }

    bool connected = false;
    for (addrinfo* rp = res; rp != nullptr && !connected; rp = rp->ai_next) {
        SOCKET s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == INVALID_SOCKET) {
            continue;
        }

        // Non-blocking
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);

        int ret = ::connect(s, rp->ai_addr, (int)rp->ai_addrlen);
        if (ret == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(s, &wfds);
                TIMEVAL tv;
                tv.tv_sec = 3;
                tv.tv_usec = 0;

                int sel = select(0, nullptr, &wfds, nullptr, &tv);
                if (sel > 0) {
                    // Check for socket error
                    int so_error = 0;
                    int optlen = sizeof(so_error);
                    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&so_error, &optlen);
                    connected = (so_error == 0);
                }
            }
        } else {
            // Immediate success
            connected = true;
        }

        closesocket(s);
    }

    freeaddrinfo(res);
    return connected;
#else
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) {
        return false;
    }

    bool connected = false;
    for (addrinfo* rp = res; rp != nullptr && !connected; rp = rp->ai_next) {
        int s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s < 0) {
            continue;
        }

        // Non-blocking
        int flags = fcntl(s, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(s, F_SETFL, flags | O_NONBLOCK);
        }

        int ret = ::connect(s, rp->ai_addr, rp->ai_addrlen);
        if (ret < 0) {
            if (errno == EINPROGRESS) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(s, &wfds);
                struct timeval tv;
                tv.tv_sec = 3;
                tv.tv_usec = 0;

                int sel = select(s + 1, nullptr, &wfds, nullptr, &tv);
                if (sel > 0) {
                    int so_error = 0;
                    socklen_t optlen = sizeof(so_error);
                    getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &optlen);
                    connected = (so_error == 0);
                }
            }
        } else {
            // Immediate success
            connected = true;
        }

        close(s);
    }

    freeaddrinfo(res);
    return connected;
#endif
}

void P2P2Functions::connect(const std::string& zHost, int zPort) {
    using org::minima::utils::messages::Message;
    using org::minima::utils::messages::MessageProcessor;

    // Build message with the NIOManager message type constant
    auto msg = std::make_shared<Message>("NIO_CONNECT");
    msg->addString("host", zHost);
    msg->addInteger("port", zPort);

    // Post to the NIOManager via the MessageProcessor base (reinterpret_cast bridge)
    auto* nio_ns = org::minima::system::Main::getInstance()->getNIOManager();
    auto* processor = reinterpret_cast<MessageProcessor*>(nio_ns);
    processor->PostMessage(msg);
}

bool P2P2Functions::checkConnect(const std::string& zHost, int zPort) {
    using org::minima::utils::MinimaLogger;
    using org::minima::system::params::GeneralParams;

    std::string hostport = zHost + ":" + std::to_string(zPort);

    // Check if added to naughty list
    if (isInvalidPeer(hostport)) {
        MinimaLogger::log("P2P2 CHECK CONNECT : Trying to connect to Invalid Peer - disallowed @ " + hostport);
        return false;
    }

    if (isIPv6(hostport)) {
        MinimaLogger::log("P2P2 CHECK CONNECT : Trying to connect to Invalid Ipv6 Peer - disallowed @ " + hostport);
        return false;
    }

    try {
        bool islocal = isIPLocal(zHost);
        if (!GeneralParams::ALLOW_ALL_IP && islocal) {
            MinimaLogger::log("[!] P2P2 not connecting to local host : " + hostport);
            return false;
        }
    } catch (...) {
        MinimaLogger::log("[-] Error getting local addresses");
    }

    // Check for already attempting connection
    std::vector<org::minima::system::network::minima::NIOClientInfo*> clients = getAllConnections();
    for (auto* client : clients) {
        if (client && !client->isConnected() && client->getHost() == zHost && client->getPort() == zPort) {
            MinimaLogger::log("Check connect failed already attempting to connect too:" + hostport);
            return false;
        }
    }

    // Do the connect
    connect(zHost, zPort);
    MinimaLogger::log("[!] P2P2 requesting NIO connection to: " + hostport);
    return true;
}

void P2P2Functions::disconnect(const std::string& zUID) {
    auto* nio = org::minima::system::Main::getInstance()->getNIOManager();
    nio->disconnect(zUID);
}

std::vector<org::minima::system::network::minima::NIOClientInfo*> P2P2Functions::getAllConnections() {
    auto* nio = org::minima::system::Main::getInstance()->getNIOManager();
    return nio->getAllConnectionInfo();
}

std::vector<org::minima::system::network::minima::NIOClientInfo*> P2P2Functions::getAllConnectedConnections() {
    std::vector<org::minima::system::network::minima::NIOClientInfo*> active;
    auto all = getAllConnections();
    for (auto* nci : all) {
        if (nci && nci->isConnected()) {
            active.push_back(nci);
        }
    }
    return active;
}

org::minima::system::network::minima::NIOClientInfo* P2P2Functions::getNIOCLientInfo(const std::string& zUID) {
    auto all = getAllConnections();
    for (auto* info : all) {
        if (info && info->getUID() == zUID) {
            return info;
        }
    }
    return nullptr;
}

std::unordered_set<std::string> P2P2Functions::getAllNetworkInterfaceAddresses() {
    std::unordered_set<std::string> hostnames;

#ifdef _WIN32
    if (!winsock_init_once()) {
        throw std::runtime_error("WSAStartup failed");
    }

    ULONG outBufLen = 15000;
    IP_ADAPTER_ADDRESSES* addresses = nullptr;
    DWORD ret = 0;

    do {
        addresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
        if (!addresses) {
            throw std::bad_alloc();
        }
        ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                               GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX,
                                   nullptr, addresses, &outBufLen);
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(addresses);
            addresses = nullptr;
        }
    } while (ret == ERROR_BUFFER_OVERFLOW);

    if (ret != NO_ERROR) {
        if (addresses) free(addresses);
        throw std::runtime_error("GetAdaptersAddresses failed");
    }

    for (IP_ADAPTER_ADDRESSES* aa = addresses; aa != nullptr; aa = aa->Next) {
        for (IP_ADAPTER_UNICAST_ADDRESS* ua = aa->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            char host[NI_MAXHOST] = {0};
            int ret_ni = getnameinfo(ua->Address.lpSockaddr, (socklen_t)ua->Address.iSockaddrLength,
                                     host, NI_MAXHOST, nullptr, 0, NI_NAMEREQD);
            if (ret_ni == 0) {
                hostnames.insert(std::string(host));
            } else {
                // Fallback to numeric if hostname not available
                ret_ni = getnameinfo(ua->Address.lpSockaddr, (socklen_t)ua->Address.iSockaddrLength,
                                     host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (ret_ni == 0) {
                    hostnames.insert(std::string(host));
                }
            }
        }
    }

    if (addresses) free(addresses);
#else
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        throw std::runtime_error("getifaddrs failed");
    }

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            char host[NI_MAXHOST] = {0};
            int s = getnameinfo(ifa->ifa_addr,
                                (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                                host, NI_MAXHOST, nullptr, 0, NI_NAMEREQD);
            if (s == 0) {
                hostnames.insert(std::string(host));
            } else {
                // Fallback to numeric host string
                s = getnameinfo(ifa->ifa_addr,
                                (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                                host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (s == 0) {
                    hostnames.insert(std::string(host));
                }
            }
        }
    }
    freeifaddrs(ifaddr);
#endif

    return hostnames;
}

int P2P2Functions::main(int /*argc*/, char* /*argv*/[]) {
    bool net = isNetAvailable();
    std::cout << "NET:" << (net ? "true" : "false") << std::endl;
    return 0;
}

} // namespace p2p2
} // namespace network
} // namespace system
} // namespace minima
} // namespace org