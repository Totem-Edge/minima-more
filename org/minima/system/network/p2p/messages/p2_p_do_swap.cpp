#include "org/minima/system/network/p2p/messages/p2_p_do_swap.hpp"

#include <any>
#include <typeinfo>
#include <cstdlib>
#include <cctype>
#include <limits>
#include <cmath>

using org::minima::objects::base::MiniData;
using org::minima::utils::json::JSONObject;

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {
namespace messages {

P2PDoSwap::P2PDoSwap()
    : m_secret(MiniData::getRandomData(8)), m_swapTarget(nullptr), m_swappingClientUID() {}

P2PDoSwap::P2PDoSwap(const MiniData& secret,
                     const InetSocketAddress& swapTarget,
                     const std::string& swappingClientUID)
    : m_secret(secret),
      m_swapTarget(std::make_unique<InetSocketAddress>(swapTarget)),
      m_swappingClientUID(swappingClientUID) {
    // In Java, a null swapTarget would throw; here reference ensures non-null.
}

P2PDoSwap P2PDoSwap::readFromJson(const JSONObject& json) {
    P2PDoSwap data;
    data.readJson(json);
    return data;
}

const std::string& P2PDoSwap::getSwappingClientUID() const {
    return m_swappingClientUID;
}

void P2PDoSwap::setSwappingClientUID(const std::string& swappingClientUID) {
    m_swappingClientUID = swappingClientUID;
}

JSONObject P2PDoSwap::toJson() const {
    if (!m_swapTarget) {
        throw std::runtime_error("swapTarget is null");
    }

    JSONObject contents;
    contents.put(SECRET_JSON_KEY, std::any(m_secret.toString()));
    contents.put(HOST_JSON_KEY, std::any(m_swapTarget->getHostString()));
    contents.put(PORT_JSON_KEY, std::any(static_cast<int>(m_swapTarget->getPort())));

    JSONObject main;
    main.put("do_swap", std::any(contents));
    return main;
}

void P2PDoSwap::readJson(const JSONObject& json) {
    if (!json.containsKey("do_swap")) {
        throw std::runtime_error("Missing 'do_swap' in JSON");
    }

    const std::any& any_contents = json.get("do_swap");
    const JSONObject* contents_ptr = nullptr;

    if (any_contents.type() == typeid(JSONObject)) {
        contents_ptr = &std::any_cast<const JSONObject&>(any_contents);
    } else if (any_contents.type() == typeid(JSONObject*)) {
        contents_ptr = std::any_cast<JSONObject*>(any_contents);
    } else {
        throw std::runtime_error("'do_swap' is not a JSONObject");
    }

    const JSONObject& contents = *contents_ptr;

    if (contents.containsKey(SECRET_JSON_KEY)) {
        std::string secstr = contents.getString(SECRET_JSON_KEY);
        setSecret(MiniData(secstr));
    }

    if (contents.containsKey(HOST_JSON_KEY) && contents.containsKey(PORT_JSON_KEY)) {
        std::string host = contents.getString(HOST_JSON_KEY);
        int port = safeReadInt(contents, PORT_JSON_KEY);
        setSwapTarget(InetSocketAddress(host, port));
    }
}

MiniData P2PDoSwap::getSecret() const {
    return m_secret;
}

void P2PDoSwap::setSecret(const MiniData& secret) {
    m_secret = secret;
}

const P2PDoSwap::InetSocketAddress* P2PDoSwap::getSwapTarget() const {
    return m_swapTarget.get();
}

void P2PDoSwap::setSwapTarget(const InetSocketAddress& swapTarget) {
    m_swapTarget = std::make_unique<InetSocketAddress>(swapTarget);
}

void P2PDoSwap::clearSwapTarget() {
    m_swapTarget.reset();
}

int P2PDoSwap::safeReadInt(const JSONObject& obj, const std::string& key) {
    const std::any& val = obj.get(key);

    if (val.type() == typeid(int)) {
        return std::any_cast<int>(val);
    }
    if (val.type() == typeid(long)) {
        long v = std::any_cast<long>(val);
        if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) {
            throw std::out_of_range("Integer out of range for key: " + key);
        }
        return static_cast<int>(v);
    }
    if (val.type() == typeid(long long)) {
        long long v = std::any_cast<long long>(val);
        if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) {
            throw std::out_of_range("Integer out of range for key: " + key);
        }
        return static_cast<int>(v);
    }
    if (val.type() == typeid(double)) {
        double d = std::any_cast<double>(val);
        double intpart = 0.0;
        if (std::modf(d, &intpart) != 0.0) {
            throw std::runtime_error("Non-integer double for key: " + key);
        }
        if (intpart < std::numeric_limits<int>::min() || intpart > std::numeric_limits<int>::max()) {
            throw std::out_of_range("Integer out of range for key: " + key);
        }
        return static_cast<int>(intpart);
    }
    if (val.type() == typeid(std::string)) {
        const std::string& s = std::any_cast<const std::string&>(val);
        // std::stoi handles leading/trailing spaces poorly; trim minimal by checking trailing chars
        size_t idx = 0;
        int res = 0;
        try {
            res = std::stoi(s, &idx, 10);
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid integer string for key: " + key);
        }
        // Ensure no trailing non-space characters
        for (; idx < s.size(); ++idx) {
            if (!std::isspace(static_cast<unsigned char>(s[idx]))) {
                throw std::runtime_error("Invalid extra characters in integer for key: " + key);
            }
        }
        return res;
    }

    throw std::runtime_error("Unsupported type for integer value for key: " + key);
}

} // namespace messages
} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org