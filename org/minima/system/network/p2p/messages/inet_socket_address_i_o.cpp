#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"

#include <stdexcept>
#include <mutex>
#include <vector>
#include <cstdlib>

#ifdef _WIN32
//   #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>
  // Link with Ws2_32.lib in your build system
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <unistd.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

namespace {

// Ensure WinSock is initialized on Windows
#ifdef _WIN32
void ensure_winsock()
{
    static std::once_flag s_wsaOnce;
    std::call_once(s_wsaOnce, [](){
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        (void)res; // If initialization fails, getaddrinfo will fail as well; we let resolution fall back gracefully.
    });
}
#endif

// Resolve host to numeric address string (IPv4/IPv6). On failure, return input host unchanged.
std::string resolve_to_numeric_host(const std::string& host)
{
#ifdef _WIN32
    ensure_winsock();
#endif

    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_ADDRCONFIG;

    addrinfo* result = nullptr;
    int ret = getaddrinfo(host.c_str(), nullptr, &hints, &result);
    if (ret != 0 || !result) {
        return host;
    }

    char hostbuf[NI_MAXHOST]{0};
    // Just pick the first address, mirroring Java InetSocketAddress default resolution behavior
    int gni = getnameinfo(result->ai_addr, static_cast<socklen_t>(result->ai_addrlen),
                          hostbuf, sizeof(hostbuf),
                          nullptr, 0,
                          NI_NUMERICHOST);
    std::string numeric = (gni == 0) ? std::string(hostbuf) : host;

    freeaddrinfo(result);
    return numeric;
}

// Split by a single-character delimiter into all tokens (naive split to mirror Java String.split(":"))
static std::vector<std::string> split_all(const std::string& s, char delim)
{
    std::vector<std::string> tokens;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            tokens.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    tokens.push_back(current);
    return tokens;
}

} // namespace

// InetAddress

InetAddress::InetAddress() : m_hostAddress() {}

InetAddress::InetAddress(const std::string& numericHost)
    : m_hostAddress(numericHost)
{
}

const std::string& InetAddress::getHostAddress() const
{
    return m_hostAddress;
}

// InetSocketAddress

InetSocketAddress::InetSocketAddress() : m_address(), m_originalHost(), m_port(0) {}

InetSocketAddress::InetSocketAddress(const std::string& host, int port)
    : m_address(resolve_to_numeric_host(host))
    , m_originalHost(host)
    , m_port(port)
{
}

const InetAddress& InetSocketAddress::getAddress() const
{
    return m_address;
}

int InetSocketAddress::getPort() const
{
    return m_port;
}

std::string InetSocketAddress::toString() const
{
    return m_address.getHostAddress() + ":" + std::to_string(m_port);
}

const std::string& InetSocketAddress::originalHost() const
{
    return m_originalHost;
}

// InetSocketAddressIO

org::minima::utils::json::JSONArray InetSocketAddressIO::addressesListToJSON(
    const std::vector<InetSocketAddress>& peers)
{
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    JSONArray array;
    if (!peers.empty()) {
        for (const auto& address : peers) {
            JSONObject obj;
            obj.put("host", std::any(address.getAddress().getHostAddress()));
            obj.put("port", std::any(static_cast<int32_t>(address.getPort())));
            array.add(std::any(obj));
        }
    }
    return array;
}

org::minima::utils::json::JSONArray InetSocketAddressIO::addressesListToJSONArray(
    const std::vector<InetSocketAddress>& peers)
{
    using org::minima::utils::json::JSONArray;

    JSONArray array;
    if (!peers.empty()) {
        for (const auto& address : peers) {
            std::string hostport = address.getAddress().getHostAddress() + ":" + std::to_string(address.getPort());
            array.add(std::any(hostport));
        }
    }
    return array;
}

std::vector<InetSocketAddress> InetSocketAddressIO::addressesJSONToList(
    const org::minima::utils::json::JSONArray& jsonArray)
{
    using org::minima::utils::json::JSONObject;

    std::vector<InetSocketAddress> peers;
    if (jsonArray.size() != 0) {
        const auto& elems = jsonArray.elements();
        peers.reserve(elems.size());
        for (const auto& a : elems) {
            const JSONObject& json = std::any_cast<const JSONObject&>(a);
            // Prefer using JSONObject's convenience getter for string if available
            std::string host;
            try {
                host = json.getString("host");
            } catch (...) {
                // Fallback to generic any-based get
                const std::any& v = json.get("host");
                host = std::any_cast<std::string>(v);
            }

            int port = safeReadInt(json, "port");
            peers.emplace_back(host, port);
        }
    }
    return peers;
}

std::vector<InetSocketAddress> InetSocketAddressIO::addressesJSONArrayToList(
    const org::minima::utils::json::JSONArray& jsonArray)
{
    std::vector<InetSocketAddress> peers;
    if (jsonArray.size() != 0) {
        const auto& elems = jsonArray.elements();
        peers.reserve(elems.size());
        for (const auto& a : elems) {
            const std::string& hostPortString = std::any_cast<const std::string&>(a);
            // Naive split, mirrors Java hostPortString.split(":")[0] and [1]
            std::vector<std::string> parts = split_all(hostPortString, ':');
            if (parts.size() < 2) {
                // Java would throw ArrayIndexOutOfBoundsException; emulate with runtime_error
                throw std::runtime_error("Invalid host:port format in JSONArray element: " + hostPortString);
            }
            const std::string& host = parts[0];
            int port = 0;
            // Java: Integer.parseInt; in C++ std::stoi throws on invalid format/out of range
            port = std::stoi(parts[1]);
            peers.emplace_back(host, port);
        }
    }
    return peers;
}

int InetSocketAddressIO::safeReadInt(const org::minima::utils::json::JSONObject& jsonObject,
                                     const std::string& key)
{
    int ret = 0;
    if (!jsonObject.containsKey(key)) {
        return ret;
    }

    const std::any& val = jsonObject.get(key);
    // Mirror Java's Integer/Long handling
    if (val.type() == typeid(int32_t)) {
        ret = std::any_cast<int32_t>(val);
    } else if (val.type() == typeid(int)) {
        ret = std::any_cast<int>(val);
    } else if (val.type() == typeid(int64_t)) {
        int64_t lv = std::any_cast<int64_t>(val);
        // Clamp like Math.toIntExact: if overflow would occur, throw
        if (lv < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) ||
            lv > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
            throw std::overflow_error("safeReadInt: value out of 32-bit range");
        }
        ret = static_cast<int>(lv);
    } else if (val.type() == typeid(long long)) {
        long long lv = std::any_cast<long long>(val);
        if (lv < static_cast<long long>(std::numeric_limits<int32_t>::min()) ||
            lv > static_cast<long long>(std::numeric_limits<int32_t>::max())) {
            throw std::overflow_error("safeReadInt: value out of 32-bit range");
        }
        ret = static_cast<int>(lv);
    } else {
        // If type is not an integer-like, default stays 0 (Java code would also not match instanceof checks)
        ret = 0;
    }
    return ret;
}

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org