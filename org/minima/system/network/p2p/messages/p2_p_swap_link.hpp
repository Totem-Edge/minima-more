#pragma once

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

class P2PSwapLink {
public:
    struct InetSocketAddress {
        std::string host;
        uint16_t port = 0;

        bool isValid() const {
            return !host.empty() || port != 0;
        }
    };

    // Constructors
    P2PSwapLink();

    // Serialization (functional equivalent of Java's Streamable)
    void writeDataStream(std::ostream& out) const;
    void readDataStream(std::istream& in);
    static P2PSwapLink ReadFromStream(std::istream& in);

    // Getters/Setters for secret
    const std::array<uint8_t, 8>& GetSecret() const;
    void SetSecret(const std::array<uint8_t, 8>& secret);

    // Getters/Setters for swap target
    const InetSocketAddress& GetSwapTarget() const;
    void SetSwapTarget(const InetSocketAddress& addr);

    // Getters/Setters for flags (not serialized in Java)
    bool GetIsSwapClientReq() const;
    void SetIsSwapClientReq(bool v);

    bool GetIsConditionalSwapReq() const;
    void SetIsConditionalSwapReq(bool v);

private:
    std::array<uint8_t, 8> m_secret;
    InetSocketAddress m_swapTarget;
    bool m_isSwapClientReq = false;
    bool m_isConditionalSwapReq = false;
};

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org