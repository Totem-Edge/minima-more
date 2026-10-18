#include "org/minima/database/wallet/script_row.hpp"

#include <utility>

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_data.hpp"

// Optionally enable exact Address/MiniData conversion if the external headers are healthy.
// Define MINIMA_HAVE_ADDRESS at build time to enable exact behavior.
#ifdef MINIMA_HAVE_ADDRESS
    #include "org/minima/objects/address.hpp"
    #include "org/minima/objects/base/mini_data.hpp"
#endif

using org::minima::utils::json::JSONObject;

ScriptRow::ScriptRow(ResultSet& zResults) {
    mScript   = zResults.getString("script");
    mAddress  = zResults.getString("address");

    int simple = zResults.getInt("simple");
    if (simple == 0) {
        mSimple = false;
    } else {
        mSimple = true;
    }

    int defv = zResults.getInt("defaultaddress");
    if (defv == 0) {
        mDefault = false;
    } else {
        mDefault = true;
    }

    mPublicKey = zResults.getString("publickey");

    int track = zResults.getInt("track");
    if (track == 0) {
        mTrack = false;
    } else {
        mTrack = true;
    }
}

ScriptRow::ScriptRow(const std::string& zScript,
                     const std::string& zAddress,
                     bool zSimple,
                     bool zDefault,
                     const std::string& zPublicKey,
                     bool zTrack)
    : mScript(zScript),
      mAddress(zAddress),
      mSimple(zSimple),
      mDefault(zDefault),
      mPublicKey(zPublicKey),
      mTrack(zTrack) {}

std::string ScriptRow::getScript() const {
    return mScript;
}

std::string ScriptRow::getAddress() const {
    return mAddress;
}

bool ScriptRow::isSimple() const {
    return mSimple;
}

bool ScriptRow::isDefault() const {
    return mDefault;
}

std::string ScriptRow::getPublicKey() const {
    return mPublicKey;
}

bool ScriptRow::isTrack() const {
    return mTrack;
}

JSONObject ScriptRow::toJSON() const {
    JSONObject ret;

    ret.put("script", getScript());
    ret.put("address", getAddress());

    org::minima::objects::base::MiniData addrData(getAddress());
    
    std::string miniaddress = org::minima::objects::Address::makeMinimaAddress(addrData);
    
    ret.put("miniaddress", miniaddress);

    ret.put("simple", isSimple());
    ret.put("default", isDefault());
    ret.put("publickey", getPublicKey());
    ret.put("track", isTrack());

    return ret;
}