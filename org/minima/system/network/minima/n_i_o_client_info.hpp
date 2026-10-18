#pragma once

#include <string>
#include <cstdint>
#include <any>

namespace org { namespace minima { namespace utils { namespace json {
class JSONObject;
}}}}

namespace org { namespace minima { namespace system { namespace network { namespace minima {
class NIOClient;
}}}}}

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

class NIOClientInfo {
public:
    // Constructors
    NIOClientInfo(const std::string& uid, const std::string& zHost, int zPort, bool zIsIncoming);
    NIOClientInfo(NIOClient* zNIOClient, bool zConnected);

    // Getters
    const std::string& getUID() const;
    bool isIncoming() const;
    bool isConnected() const;
    const std::string& getHost() const;
    int getPort() const;
    int getMinimaPort() const;

    // Delegated info (requires mNIOClient to be non-null)
    long long getTimeConnected() const;
    std::any getExtraData() const;
    void setExtrasData(const std::any& zExtraData);

    // JSON
    org::minima::utils::json::JSONObject toJSON() const;

    // Valid greeting
    bool ismValidGreeting() const;

private:
    // Non-owning pointer to NIOClient (may be nullptr)
    NIOClient* mNIOClient = nullptr;

    bool mConnected = false;

    std::string mWelcome = "no welcome set..";

    std::string mUID;
    std::string mHost;

    int mPort = 0;
    int mMinimaPort = 0;

    bool mIsIncoming = false;

    bool mValidGreeting = false;
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org