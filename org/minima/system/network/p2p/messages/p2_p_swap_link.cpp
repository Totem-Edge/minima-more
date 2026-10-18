#include "org/minima/system/network/p2p/messages/p2_p_swap_link.hpp"

#include <cstring>
#include <exception>
#include <iomanip>
#include <ios>
#include <istream>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

namespace {

// Big-endian write helpers
void writeUint16BE(std::ostream& out, uint16_t v) {
    uint8_t buf[2] = {
        static_cast<uint8_t>((v >> 8) & 0xff),
        static_cast<uint8_t>(v & 0xff)
    };
    out.write(reinterpret_cast<const char*>(buf), 2);
    if (!out) {
        throw std::runtime_error("P2PSwapLink: failed to write uint16");
    }
}

void writeUint32BE(std::ostream& out, uint32_t v) {
    uint8_t buf[4] = {
        static_cast<uint8_t>((v >> 24) & 0xff),
        static_cast<uint8_t>((v >> 16) & 0xff),
        static_cast<uint8_t>((v >> 8) & 0xff),
        static_cast<uint8_t>(v & 0xff)
    };
    out.write(reinterpret_cast<const char*>(buf), 4);
    if (!out) {
        throw std::runtime_error("P2PSwapLink: failed to write uint32");
    }
}

uint16_t readUint16BE(std::istream& in) {
    uint8_t buf[2];
    in.read(reinterpret_cast<char*>(buf), 2);
    if (!in) {
        throw std::runtime_error("P2PSwapLink: failed to read uint16");
    }
    return static_cast<uint16_t>((buf[0] << 8) | buf[1]);
}

uint32_t readUint32BE(std::istream& in) {
    uint8_t buf[4];
    in.read(reinterpret_cast<char*>(buf), 4);
    if (!in) {
        throw std::runtime_error("P2PSwapLink: failed to read uint32");
    }
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8) |
            static_cast<uint32_t>(buf[3]);
}

} // anonymous namespace

P2PSwapLink::P2PSwapLink() {
    // Initialize secret with 8 random bytes (equivalent to MiniData.getRandomData(8))
    std::random_device rd;
    for (auto& b : m_secret) {
        b = static_cast<uint8_t>(rd());
    }
}

void P2PSwapLink::writeDataStream(std::ostream& out) const {
    // Write secret: 8 raw bytes
    out.write(reinterpret_cast<const char*>(m_secret.data()), m_secret.size());
    if (!out) {
        throw std::runtime_error("P2PSwapLink: failed to write secret bytes");
    }

    // Write swap target using a clear, deterministic format:
    // [present: uint8_t] if 1 then [host_len: uint32 BE][host bytes][port: uint16 BE]
    uint8_t present = m_swapTarget.isValid() ? 1 : 0;
    out.write(reinterpret_cast<const char*>(&present), 1);
    if (!out) {
        throw std::runtime_error("P2PSwapLink: failed to write swap target presence flag");
    }

    if (present) {
        const std::string& host = m_swapTarget.host;
        if (host.size() > 0xFFFFFFFFu) {
            throw std::runtime_error("P2PSwapLink: host too long to serialize");
        }
        writeUint32BE(out, static_cast<uint32_t>(host.size()));
        if (!host.empty()) {
            out.write(host.data(), static_cast<std::streamsize>(host.size()));
            if (!out) {
                throw std::runtime_error("P2PSwapLink: failed to write host");
            }
        }
        writeUint16BE(out, m_swapTarget.port);
    }
}

void P2PSwapLink::readDataStream(std::istream& in) {
    // Read secret: 8 raw bytes
    in.read(reinterpret_cast<char*>(m_secret.data()), m_secret.size());
    if (!in) {
        throw std::runtime_error("P2PSwapLink: failed to read secret bytes");
    }

    // Read swap target presence
    uint8_t present = 0;
    in.read(reinterpret_cast<char*>(&present), 1);
    if (!in) {
        throw std::runtime_error("P2PSwapLink: failed to read swap target presence flag");
    }

    if (present) {
        uint32_t hlen = readUint32BE(in);
        std::string host;
        host.resize(hlen);
        if (hlen > 0) {
            in.read(&host[0], static_cast<std::streamsize>(hlen));
            if (!in) {
                throw std::runtime_error("P2PSwapLink: failed to read host");
            }
        }
        uint16_t port = readUint16BE(in);
        m_swapTarget.host = std::move(host);
        m_swapTarget.port = port;
    } else {
        m_swapTarget.host.clear();
        m_swapTarget.port = 0;
    }
}

P2PSwapLink P2PSwapLink::ReadFromStream(std::istream& in) {
    P2PSwapLink data;
    data.readDataStream(in);
    return data;
}

const std::array<uint8_t, 8>& P2PSwapLink::GetSecret() const {
    return m_secret;
}

void P2PSwapLink::SetSecret(const std::array<uint8_t, 8>& secret) {
    m_secret = secret;
}

const P2PSwapLink::InetSocketAddress& P2PSwapLink::GetSwapTarget() const {
    return m_swapTarget;
}

void P2PSwapLink::SetSwapTarget(const InetSocketAddress& addr) {
    m_swapTarget = addr;
}

bool P2PSwapLink::GetIsSwapClientReq() const {
    return m_isSwapClientReq;
}

void P2PSwapLink::SetIsSwapClientReq(bool v) {
    m_isSwapClientReq = v;
}

bool P2PSwapLink::GetIsConditionalSwapReq() const {
    return m_isConditionalSwapReq;
}

void P2PSwapLink::SetIsConditionalSwapReq(bool v) {
    m_isConditionalSwapReq = v;
}

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org