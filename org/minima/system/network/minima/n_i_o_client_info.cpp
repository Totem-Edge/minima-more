#include "org/minima/system/network/minima/n_i_o_client_info.hpp"

#include "org/minima/system/network/minima/n_i_o_client.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace {

std::string formatDateFromMillis(long long millisSinceEpoch) {
    using namespace std::chrono;
    system_clock::time_point tp = system_clock::time_point(milliseconds(millisSinceEpoch));
    std::time_t tt = system_clock::to_time_t(tp);

    std::tm tmres{};
#ifdef _WIN32
    localtime_s(&tmres, &tt);
#else
    localtime_r(&tt, &tmres);
#endif

    // Approximate Java Date.toString() format: "Wed Oct 26 14:19:00 GMT 2025"
    std::ostringstream oss;
    oss << std::put_time(&tmres, "%a %b %d %H:%M:%S %Z %Y");
    return oss.str();
}

} // anonymous namespace

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

NIOClientInfo::NIOClientInfo(const std::string& uid, const std::string& zHost, int zPort, bool zIsIncoming)
    : mNIOClient(nullptr)
    , mConnected(true)
    , mWelcome("no welcome set..")
    , mUID(uid)
    , mHost(zHost)
    , mPort(zPort)
    , mMinimaPort(0)
    , mIsIncoming(zIsIncoming)
    , mValidGreeting(false) {
}

NIOClientInfo::NIOClientInfo(NIOClient* zNIOClient, bool zConnected)
    : mNIOClient(zNIOClient)
    , mConnected(zConnected) {
    // Pull values from the NIOClient
    mWelcome        = mNIOClient->getWelcomeMessage();
    mValidGreeting  = mNIOClient->isValidGreeting();
    mHost           = mNIOClient->getHost();
    mPort           = mNIOClient->getPort();
    mMinimaPort     = mNIOClient->getMinimaPort();
    mUID            = mNIOClient->getUID();
    mIsIncoming     = mNIOClient->isIncoming();
}

const std::string& NIOClientInfo::getUID() const {
    return mUID;
}

bool NIOClientInfo::isIncoming() const {
    return mIsIncoming;
}

bool NIOClientInfo::isConnected() const {
    return mConnected;
}

const std::string& NIOClientInfo::getHost() const {
    return mHost;
}

int NIOClientInfo::getPort() const {
    return mPort;
}

int NIOClientInfo::getMinimaPort() const {
    return mMinimaPort;
}

long long NIOClientInfo::getTimeConnected() const {
    // Mirrors Java behavior; assumes mNIOClient is valid when used.
    return static_cast<long long>(mNIOClient->getTimeConnected());
}

std::any NIOClientInfo::getExtraData() const {
    // Mirrors Java behavior; assumes mNIOClient is valid when used.
    return mNIOClient->getExtraData();
}

void NIOClientInfo::setExtrasData(const std::any& zExtraData) {
    // Mirrors Java behavior; assumes mNIOClient is valid when used.
    mNIOClient->setExtraData(zExtraData);
}

org::minima::utils::json::JSONObject NIOClientInfo::toJSON() const {
    org::minima::utils::json::JSONObject ret;

    ret.put("welcome", mWelcome);
    ret.put("uid", mUID);
    ret.put("incoming", mIsIncoming);
    ret.put("host", mHost);
    ret.put("port", mPort);
    ret.put("minimaport", mMinimaPort);
    ret.put("isconnected", mConnected);
    ret.put("valid", mValidGreeting);

    if (mNIOClient != nullptr) {
        ret.put("connected", formatDateFromMillis(getTimeConnected()));
    }

    return ret;
}

bool NIOClientInfo::ismValidGreeting() const {
    return mValidGreeting;
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org