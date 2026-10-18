#include "org/minima/system/network/p2p/p2_p_functions.hpp"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <memory>

// Project headers
#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/network/p2p/params/p2_p_params.hpp"
#include "org/minima/utils/messages/message.hpp"

// Platform-specific networking
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  // Avoid macro clash with our MessageProcessor::PostMessage
  #ifdef PostMessage
    #undef PostMessage
  #endif
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <ifaddrs.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

// Static members
std::unique_ptr<std::unordered_set<std::string>> P2PFunctions::s_localAddresses;
std::unordered_set<std::string> P2PFunctions::s_invalidPeers;

const std::unordered_set<std::string>& P2PFunctions::getLocalAddresses() {
    if (!s_localAddresses) {
        try {
            auto addrs = getAllNetworkInterfaceAddresses();
            s_localAddresses = std::make_unique<std::unordered_set<std::string>>(std::move(addrs));
            if (s_localAddresses->empty()) {
                // Fallback defaults if enumeration returned nothing
                s_localAddresses->insert("localhost");
                s_localAddresses->insert("127.0.0.1");
                s_localAddresses->insert("127.0.1.1");
            }
        } catch (const std::exception&) {
            s_localAddresses = std::make_unique<std::unordered_set<std::string>>();
            s_localAddresses->insert("localhost");
            s_localAddresses->insert("127.0.0.1");
            s_localAddresses->insert("127.0.1.1");
        }
    }
    return *s_localAddresses;
}

void P2PFunctions::addInvalidPeer(const std::string& zHostPort) {
    if (s_invalidPeers.find(zHostPort) == s_invalidPeers.end()) {
        org::minima::utils::MinimaLogger::log(std::string("INVALID PEER added to List! ") + zHostPort);
        s_invalidPeers.insert(zHostPort);
    } else {
        // Already added; no log (mirrors commented Java log)
    }
}

bool P2PFunctions::isInvalidPeer(const std::string& zHostPort) {
    return s_invalidPeers.find(zHostPort) != s_invalidPeers.end();
}

void P2PFunctions::clearInvalidPeers() {
    s_invalidPeers.clear();
}

bool P2PFunctions::isIPv6(const std::string& fullhost) {
    auto first = fullhost.find(':');
    auto last  = fullhost.rfind(':');
    return (first != std::string::npos) && (last != std::string::npos) && (first != last);
}

bool P2PFunctions::isIPLocal(const std::string& fullhost) {
    if (fullhost.rfind("localhost", 0) == 0) {
        return true;
    }

    // Use inet_pton to parse IPv4; then check RFC 1918 / loopback ranges precisely.
    struct in_addr addr4{};
    if (inet_pton(AF_INET, fullhost.c_str(), &addr4) == 1) {
        uint32_t ip = ntohl(addr4.s_addr);
        // 127.0.0.0/8
        if ((ip & 0xFF000000U) == 0x7F000000U) return true;
        // 10.0.0.0/8
        if ((ip & 0xFF000000U) == 0x0A000000U) return true;
        // 172.16.0.0/12
        if ((ip & 0xFFF00000U) == 0xAC100000U) return true;
        // 192.168.0.0/16
        if ((ip & 0xFFFF0000U) == 0xC0A80000U) return true;
    }

    // IPv6 loopback / unique-local (fc00::/7)
    struct in6_addr addr6{};
    if (inet_pton(AF_INET6, fullhost.c_str(), &addr6) == 1) {
        if (IN6_IS_ADDR_LOOPBACK(&addr6)) return true;
        uint8_t first = addr6.s6_addr[0];
        if ((first & 0xFE) == 0xFC) return true; // fc00::/7
    }

    return false;
}

bool P2PFunctions::isNetAvailable() {
#ifdef _WIN32
    WSADATA wsaData;
    bool wsaInit = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

    const char* host = "www.google.com";
    const char* service = "443";

    struct addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res = nullptr;
    int gai = getaddrinfo(host, service, &hints, &res);
    if (gai != 0 || !res) {
#ifdef _WIN32
        if (wsaInit) WSACleanup();
#endif
        return false;
    }

    bool connected = false;
    for (struct addrinfo* rp = res; rp != nullptr; rp = rp->ai_next) {
#ifdef _WIN32
        SOCKET s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == INVALID_SOCKET) continue;

        // Set non-blocking and attempt connect with timeout (~3s)
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);
        int c = ::connect(s, rp->ai_addr, static_cast<int>(rp->ai_addrlen));
        if (c == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEINVAL) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(s, &wfds);
                timeval tv{3, 0};
                int sel = select(0, nullptr, &wfds, nullptr, &tv);
                if (sel > 0 && FD_ISSET(s, &wfds)) {
                    connected = true;
                }
            }
        } else {
            connected = true;
        }
        closesocket(s);
        if (connected) break;
#else
        int s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s < 0) continue;

        // Use a basic connect with a send timeout to avoid long blocking
        timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        int c = ::connect(s, rp->ai_addr, rp->ai_addrlen);
        if (c == 0) {
            connected = true;
        }
        ::close(s);
        if (connected) break;
#endif
    }

    freeaddrinfo(res);
#ifdef _WIN32
    if (wsaInit) WSACleanup();
#endif
    return connected;
}

void P2PFunctions::connect(const std::string& zHost, int zPort) {
    // Construct and send a Message to NIOManager
    auto spmsg = std::make_shared<org::minima::utils::messages::Message>(
        org::minima::system::network::minima::NIOManager::NIO_CONNECT);
    spmsg->addString("host", zHost);
    spmsg->addInteger("port", zPort);

    org::minima::system::Main::getInstance()->getNIOManager().PostMessage(spmsg);
}

bool P2PFunctions::checkConnect(const std::string& zHost, int zPort) {
    // Build the message like Java (even if we only send on success)
    org::minima::utils::messages::Message msg(org::minima::system::network::minima::NIOManager::NIO_CONNECT);
    msg.addString("host", zHost);
    msg.addInteger("port", zPort);

    // Check invalid peers list
    {
        std::ostringstream hp;
        hp << zHost << ":" << zPort;
        if (isInvalidPeer(hp.str())) {
            org::minima::utils::MinimaLogger::log("P2P CHECK CONNECT : Trying to connect to Invalid Peer - disallowed @ " + hp.str());
            return false;
        }
        if (isIPv6(hp.str())) {
            org::minima::utils::MinimaLogger::log("P2P CHECK CONNECT : Trying to connect to Invalid Ipv6 Peer - disallowed @ " + hp.str());
            return false;
        }
    }

    bool doConnect = true;
    try {
        bool islocal = isIPLocal(zHost);
        if (!org::minima::system::params::GeneralParams::ALLOW_ALL_IP && islocal) {
            P2PFunctions::log_debug(std::string("[!] P2P not connecting to local host : ") + zHost + ":" + std::to_string(zPort));
            return false;
        }
        // The Java code had extra checks commented out; preserved behavior
    } catch (const std::exception&) {
        org::minima::utils::MinimaLogger::log("[-] Error getting local addresses");
    }

    // Ensure we are not already attempting to connect to the same host:port
    {
        auto infos = org::minima::system::Main::getInstance()->getNIOManager().getAllConnectionInfo();
        for (const auto& uptr : infos) {
            const auto& client = *uptr;
            if (!client.isConnected() && client.getHost() == zHost && client.getPort() == zPort) {
                org::minima::utils::MinimaLogger::log("Check connect failed already attempting to connect too:" + zHost + ":" + std::to_string(zPort));
                return false;
            }
        }
    }

    if (doConnect) {
        connect(zHost, zPort);
        P2PFunctions::log_debug(std::string("[!] P2P requesting NIO connection to: ") + zHost + ":" + std::to_string(zPort));
    }
    return doConnect;
}

void P2PFunctions::disconnect(const std::string& zUID) {
    org::minima::system::Main::getInstance()->getNIOManager().disconnect(zUID);
}

std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>>
P2PFunctions::getAllConnections() {
    // Delegate to NIOManager. Ownership semantics are determined by that API.
    return org::minima::system::Main::getInstance()->getNIOManager().getAllConnectionInfo();
}

std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>>
P2PFunctions::getAllConnectedConnections() {
    std::vector<std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>> activeConnections;
    auto infos = org::minima::system::Main::getInstance()->getNIOManager().getAllConnectionInfo();
    for (const auto& uptr : infos) {
        if (uptr && uptr->isConnected()) {
            activeConnections.push_back(std::make_unique<org::minima::system::network::minima::NIOClientInfo>(*uptr));
        }
    }
    return activeConnections;
}

std::unique_ptr<org::minima::system::network::minima::NIOClientInfo>
P2PFunctions::getNIOCLientInfo(const std::string& zUID) {
    auto allclients = getAllConnections();
    for (const auto& info : allclients) {
        if (info && info->getUID() == zUID) {
            return std::make_unique<org::minima::system::network::minima::NIOClientInfo>(*info);
        }
    }
    return nullptr;
}

void P2PFunctions::sendP2PMessage(const std::string& zUID, const org::minima::utils::json::JSONObject& zMessage) {
    // Convert the message to a streamable MiniString
    org::minima::objects::base::MiniString json(zMessage.toString());

    // Send via NIOManager with P2P message type
    org::minima::objects::base::MiniByte msgtype(org::minima::system::network::minima::NIOMessage::MSG_P2P());
    org::minima::system::network::minima::NIOManager::sendNetworkMessage(zUID, msgtype, json);
}

void P2PFunctions::sendP2PMessageAll(const org::minima::utils::json::JSONObject& zMessage) {
    // Java calls sendP2PMessage("", zMessage)
    sendP2PMessage("", zMessage);
}

void P2PFunctions::log_node_runner(const std::string& message) {
    if (org::minima::system::network::p2p::params::P2PParams::LOG_LEVEL == Level::NODE_RUNNER_MSG) {
        org::minima::utils::MinimaLogger::log(std::string("[P2P] ") + message);
    }
}

void P2PFunctions::log_info(const std::string& message) {
    auto lvl = org::minima::system::network::p2p::params::P2PParams::LOG_LEVEL;
    if (lvl == Level::INFO || lvl == Level::DEBUG) {
        org::minima::utils::MinimaLogger::log(std::string("[I] ") + message);
    }
}

void P2PFunctions::log_debug(const std::string& message) {
    if (org::minima::system::network::p2p::params::P2PParams::LOG_LEVEL == Level::DEBUG) {
        org::minima::utils::MinimaLogger::log(std::string("[D] ") + message);
    }
}

std::unordered_set<std::string> P2PFunctions::getAllNetworkInterfaceAddresses() {
    std::unordered_set<std::string> hostnames;

#ifdef _WIN32
    // Best-effort: get host addresses via getaddrinfo on the local hostname
    WSADATA wsaData;
    bool wsaInit = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);

    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* res = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &res) == 0) {
            for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
                char hbuf[NI_MAXHOST] = {0};
                if (getnameinfo(p->ai_addr, static_cast<socklen_t>(p->ai_addrlen),
                                hbuf, sizeof(hbuf), nullptr, 0, NI_NUMERICHOST) == 0) {
                    hostnames.insert(std::string(hbuf));
                }
            }
            freeaddrinfo(res);
        }
    }

    if (wsaInit) WSACleanup();
#else
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            int family = ifa->ifa_addr->sa_family;
            if (family == AF_INET || family == AF_INET6) {
                char host[NI_MAXHOST] = {0};
                int rc = getnameinfo(ifa->ifa_addr,
                                     (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                                     host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (rc == 0) {
                    hostnames.insert(std::string(host));
                }
            }
        }
        freeifaddrs(ifaddr);
    }
#endif

    return hostnames;
}

void P2PFunctions::main(const std::vector<std::string>& /*zArgs*/) {
    std::cout << "NET:" << (isNetAvailable() ? "true" : "false") << std::endl;
}

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org