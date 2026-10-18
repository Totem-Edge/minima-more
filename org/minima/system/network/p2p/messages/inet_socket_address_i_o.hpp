#pragma once

#include <string>
#include <vector>
#include <any>
#include <cstdint>

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

// Minimal InetAddress analogue to support getHostAddress()
class InetAddress {
public:
    InetAddress();
    explicit InetAddress(const std::string& numericHost);
    const std::string& getHostAddress() const;

private:
    std::string m_hostAddress;
};

// Minimal InetSocketAddress analogue to support getAddress().getHostAddress() and getPort()
struct InetSocketAddress {
public:
    InetSocketAddress();
    InetSocketAddress(const std::string& host, int port);

    bool operator==(const InetSocketAddress& other) const {
        // Assumes getAddress() and getPort() are const
        return getAddress().getHostAddress() == other.getAddress().getHostAddress() &&
            getPort() == other.getPort();
    }

    const InetAddress& getAddress() const;
    int getPort() const;

    // Convenience string representation matching java.net.InetSocketAddress.toString()
    std::string toString() const;

    // Access to original host string (unresolved) if ever needed by callers
    const std::string& originalHost() const;

private:
    InetAddress m_address;     // resolved numeric host address (if resolution fails, stores original)
    std::string m_originalHost;
    int m_port{0};
};

class InetSocketAddressIO {
public:
    // Static-only utility class
    InetSocketAddressIO() = delete;

    // Java: public static JSONArray addressesListToJSON(List<InetSocketAddress> peers)
    static org::minima::utils::json::JSONArray addressesListToJSON(
        const std::vector<InetSocketAddress>& peers);

    // Java: public static JSONArray addressesListToJSONArray(List<InetSocketAddress> peers)
    static org::minima::utils::json::JSONArray addressesListToJSONArray(
        const std::vector<InetSocketAddress>& peers);

    // Java: public static List<InetSocketAddress> addressesJSONToList(JSONArray jsonArray)
    static std::vector<InetSocketAddress> addressesJSONToList(
        const org::minima::utils::json::JSONArray& jsonArray);

    // Java: public static List<InetSocketAddress> addressesJSONArrayToList(JSONArray jsonArray)
    static std::vector<InetSocketAddress> addressesJSONArrayToList(
        const org::minima::utils::json::JSONArray& jsonArray);

    // Java: public static int safeReadInt(JSONObject jsonObject, String key)
    static int safeReadInt(const org::minima::utils::json::JSONObject& jsonObject,
                           const std::string& key);
};

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org