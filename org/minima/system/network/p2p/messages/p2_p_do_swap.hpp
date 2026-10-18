#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <utility>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

class P2PDoSwap {
public:
    // Minimal InetSocketAddress equivalent for this context
    struct InetSocketAddress {
        std::string m_host;
        int m_port{0};

        InetSocketAddress() = default;
        InetSocketAddress(std::string host, int port) : m_host(std::move(host)), m_port(port) {}
        const std::string& getHostString() const { return m_host; }
        int getPort() const { return m_port; }
    };

    // Constants for JSON keys
    static constexpr const char* SECRET_JSON_KEY = "secret";
    static constexpr const char* HOST_JSON_KEY   = "swap_target_host";
    static constexpr const char* PORT_JSON_KEY   = "swap_target_port";

    // Constructors
    P2PDoSwap(); // default for reading from JSON
    P2PDoSwap(const org::minima::objects::base::MiniData& secret,
              const InetSocketAddress& swapTarget,
              const std::string& swappingClientUID);

    // Special members
    ~P2PDoSwap() = default;
    P2PDoSwap(P2PDoSwap&&) noexcept = default;
    P2PDoSwap& operator=(P2PDoSwap&&) noexcept = default;
    P2PDoSwap(const P2PDoSwap&) = delete;
    P2PDoSwap& operator=(const P2PDoSwap&) = delete;

    // Static factory (JSON)
    static P2PDoSwap readFromJson(const org::minima::utils::json::JSONObject& json);

    // Accessors
    const std::string& getSwappingClientUID() const;
    void setSwappingClientUID(const std::string& swappingClientUID);

    // JSON conversion
    org::minima::utils::json::JSONObject toJson() const;
    void readJson(const org::minima::utils::json::JSONObject& json);

    // Secret
    org::minima::objects::base::MiniData getSecret() const;
    void setSecret(const org::minima::objects::base::MiniData& secret);

    // Swap target (nullable)
    const InetSocketAddress* getSwapTarget() const;
    void setSwapTarget(const InetSocketAddress& swapTarget);
    void clearSwapTarget(); // set to null

private:
    org::minima::objects::base::MiniData m_secret;
    std::unique_ptr<InetSocketAddress> m_swapTarget;
    std::string m_swappingClientUID;

    static int safeReadInt(const org::minima::utils::json::JSONObject& obj, const std::string& key);
};

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org