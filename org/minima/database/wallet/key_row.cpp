#include "org/minima/database/wallet/key_row.hpp"

namespace org {
namespace minima {
namespace database {
namespace wallet {

KeyRow::KeyRow(ResultSetReader& zResults) {
    // Mirror Java field loading and column names exactly
    mSize       = zResults.getInt("size");
    mDepth      = zResults.getInt("depth");
    mUses       = zResults.getInt("uses");
    mMaxUses    = zResults.getInt("maxuses");
    mModifier   = zResults.getString("modifier");
    mPrivateKey = zResults.getString("privatekey");
    mPublicKey  = zResults.getString("publickey");
}

KeyRow::KeyRow(int zSize,
               int zDepth,
               int zUses,
               int zMaxUses,
               const std::string& zModifier,
               const std::string& zPrivate,
               const std::string& zPublic)
    : mSize(zSize)
    , mDepth(zDepth)
    , mUses(zUses)
    , mMaxUses(zMaxUses)
    , mModifier(zModifier)
    , mPublicKey(zPublic)
    , mPrivateKey(zPrivate) {
}

int KeyRow::getSize() const {
    return mSize;
}

int KeyRow::getDepth() const {
    return mDepth;
}

int KeyRow::getUses() const {
    return mUses;
}

int KeyRow::getMaxUses() const {
    return mMaxUses;
}

const std::string& KeyRow::getModifier() const {
    return mModifier;
}

const std::string& KeyRow::getPrivateKey() const {
    return mPrivateKey;
}

const std::string& KeyRow::getPublicKey() const {
    return mPublicKey;
}

org::minima::utils::json::JSONObject KeyRow::toJSON() const {
    org::minima::utils::json::JSONObject ret;
    ret.put("size", mSize);
    ret.put("depth", mDepth);
    ret.put("uses", mUses);
    ret.put("maxuses", mMaxUses);
    ret.put("modifier", getModifier());
    // Do not include privatekey in JSON (matches Java's commented-out line)
    ret.put("publickey", getPublicKey());
    return ret;
}

} // namespace wallet
} // namespace database
} // namespace minima
} // namespace org