#include "org/minima/objects/coin.hpp"

#include <sstream>
#include <stdexcept>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/token.hpp"

// DO NOT include address.hpp here to avoid signature conflicts; we will not directly reference Address here.

using org::minima::objects::Coin;
using org::minima::objects::StateVariable;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniByte;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

// Static constants
const MiniData Coin::COINID_OUTPUT = MiniData("0x00");
const MiniData Coin::COINID_ELTOO  = MiniData("0x01");

// Private default constructor
Coin::Coin()
    : mCoinID(),
      mAddress(),
      mAmount(MiniNumber::ZERO()),
      mTokenID(),
      mStoreState(true),
      mState(),
      mMMREntryNumber(MMREntryNumber::ZERO),
      mSpent(MiniByte::FALSE()),
      mBlockCreated(MiniNumber::ZERO()),
      mToken(nullptr) {}

// Public constructors
Coin::Coin(const MiniData& zAddress,
           const MiniNumber& zAmount,
           const MiniData& zTokenID)
    : Coin(COINID_OUTPUT, zAddress, zAmount, zTokenID, true) {}

Coin::Coin(const MiniData& zAddress,
           const MiniNumber& zAmount,
           const MiniData& zTokenID,
           bool zStoreState)
    : Coin(COINID_OUTPUT, zAddress, zAmount, zTokenID, zStoreState) {}

Coin::Coin(const MiniData& zCoinID,
           const MiniData& zAddress,
           const MiniNumber& zAmount,
           const MiniData& zTokenID)
    : Coin(zCoinID, zAddress, zAmount, zTokenID, true) {}

Coin::Coin(const MiniData& zCoinID,
           const MiniData& zAddress,
           const MiniNumber& zAmount,
           const MiniData& zTokenID,
           bool zStoreState)
    : mCoinID(zCoinID),
      mAddress(zAddress),
      mAmount(zAmount),
      mTokenID(zTokenID),
      mStoreState(zStoreState),
      mState(),
      mMMREntryNumber(MMREntryNumber::ZERO),
      mSpent(MiniByte::FALSE()),
      mBlockCreated(MiniNumber::ZERO()),
      mToken(nullptr) {}

// Destructor and special members (PITFALL 1)
Coin::~Coin() = default;
Coin::Coin(Coin&&) noexcept = default;
Coin& Coin::operator=(Coin&&) noexcept = default;


Coin::Coin(const Coin& zOther)
    : mCoinID(zOther.mCoinID),
      mAddress(zOther.mAddress),
      mAmount(zOther.mAmount),
      mTokenID(zOther.mTokenID),
      mStoreState(zOther.mStoreState),
      mMMREntryNumber(zOther.mMMREntryNumber),
      mSpent(zOther.mSpent),
      mBlockCreated(zOther.mBlockCreated)
{
    // Manually deep-copy the vector of unique_ptrs
    mState.reserve(zOther.mState.size());
    for (const auto& sv : zOther.mState) {
        // This assumes StateVariable has a copy constructor
        mState.push_back(std::make_unique<StateVariable>(*sv));
    }

    // Manually deep-copy the unique_ptr for mToken
    if (zOther.mToken) {
        // This assumes Token has a copy constructor
        mToken = std::make_unique<Token>(*zOther.mToken);
    } else {
        mToken = nullptr;
    }
}

// API methods
std::unique_ptr<Coin> Coin::getSameCoinWithCoinID(const MiniData& zCoinID) const {
    auto copy = deepCopy();
    if (copy) {
        copy->resetCoinID(zCoinID);
    }
    return copy;
}

void Coin::resetCoinID(const MiniData& zCoinID) { mCoinID = zCoinID; }
void Coin::resetTokenID(const MiniData& zTokenID) { mTokenID = zTokenID; }

const Token* Coin::getToken() const { return mToken.get(); }
Token* Coin::getToken() { return mToken.get(); }

void Coin::setToken(std::unique_ptr<Token> zToken) {
    mToken = std::move(zToken);
}

void Coin::setMMREntryNumber(const MMREntryNumber& zEntryNumber) { mMMREntryNumber = zEntryNumber; }
MMREntryNumber Coin::getMMREntryNumber() const { return mMMREntryNumber; }

void Coin::setSpent(bool zSpent) { mSpent = MiniByte(zSpent); }
bool Coin::getSpent() const { return mSpent.isTrue(); }

void Coin::setBlockCreated(const MiniNumber& zBlock) { mBlockCreated = zBlock; }
MiniNumber Coin::getBlockCreated() const { return mBlockCreated; }

bool Coin::storeState() const { return mStoreState; }

MiniData Coin::getCoinID() const { return mCoinID; }
MiniData Coin::getAddress() const { return mAddress; }
MiniNumber Coin::getAmount() const { return mAmount; }
MiniData Coin::getTokenID() const { return mTokenID; }

MiniNumber Coin::getTokenAmount() const {
    if (!getToken()) {
        return getAmount();
    }
    auto scaled = getToken()->getScaledTokenAmount(getAmount());
    return scaled ? *scaled : MiniNumber::ZERO();
}

std::vector<std::unique_ptr<StateVariable>>& Coin::getState() { return mState; }
const std::vector<std::unique_ptr<StateVariable>>& Coin::getState() const { return mState; }

void Coin::setState(std::vector<std::unique_ptr<StateVariable>> zCompleteState) {
    mState = std::move(zCompleteState);
}

bool Coin::checkForStateVariable(const std::string& zCheckState) const {
    return checkForStateVariable(zCheckState, false);
}

bool Coin::checkForStateVariable(const std::string& zCheckState, bool zWildcard) const {
    for (const auto& svptr : mState) {
        const StateVariable& sv = *svptr;
        std::string val = sv.getData().toString();
        if (zWildcard) {
            if (val.find(zCheckState) != std::string::npos) {
                return true;
            }
        } else {
            if (val == zCheckState) {
                return true;
            }
        }
    }
    return false;
}

std::string Coin::toString() const {
    return toJSON().toString();
}

JSONObject Coin::toJSON() const {
    return toJSON(false);
}

JSONObject Coin::toJSON(bool zSimpleState) const {
    JSONObject obj;

    obj.put("coinid", mCoinID.toString());
    obj.put("amount", mAmount.toString());

    obj.put("address", mAddress.toString());
    // NOTE: Avoid including Address.hpp here due to conflicting signatures elsewhere.
    // We include a placeholder using the address string representation.
    obj.put("miniaddress", mAddress.toString());

    obj.put("tokenid", mTokenID.toString());
    if (!mToken) {
        obj.put("token", std::nullptr_t(nullptr));
    } else {
        auto tokjson = mToken->toJSON();
        obj.put("token", *tokjson);
        auto tokenamtptr = getToken()->getScaledTokenAmount(getAmount());
        MiniNumber tokenamt = tokenamtptr ? *tokenamtptr : MiniNumber::ZERO();
        obj.put("tokenamount", tokenamt.toString());
    }

    obj.put("storestate", mStoreState);

    if (zSimpleState) {
        JSONObject state;
        for (const auto& svptr : mState) {
            const StateVariable& sv = *svptr;
            state.put(std::to_string(sv.getPort()), sv.getData().toString());
        }
        obj.put("state", state);
    } else {
        JSONArray starr;
        for (const auto& svptr : mState) {
            const StateVariable& sv = *svptr;
            starr.add(sv.toJSON());
        }
        obj.put("state", starr);
    }

    obj.put("spent", mSpent.isTrue());
    obj.put("mmrentry", mMMREntryNumber.toString());
    obj.put("created", mBlockCreated.toString());

    return obj;
}

JSONObject Coin::getStateAsJSON() const {
    JSONObject state;
    for (const auto& svptr : mState) {
        const StateVariable& sv = *svptr;
        state.put(std::to_string(sv.getPort()), sv.getData().toString());
    }
    return state;
}

JSONObject Coin::convertStateListToJSON(const std::vector<std::unique_ptr<StateVariable>>& zStateList) {
    JSONObject state;
    for (const auto& svptr : zStateList) {
        const StateVariable& sv = *svptr;
        state.put(std::to_string(sv.getPort()), sv.getData().toString());
    }
    return state;
}

std::unique_ptr<Coin> Coin::convertMiniDataVersion(const MiniData& zTxpData) {
    try {
        const auto& bytes = zTxpData.getBytes();
        std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::istringstream in(buf, std::ios::binary);

        auto coin = Coin::ReadFromStream(in);
        return coin;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
    return nullptr;
}

std::unique_ptr<Coin> Coin::deepCopy() const {
    try {
        std::ostringstream out(std::ios::binary);
        const_cast<Coin*>(this)->writeDataStream(out);
        std::string data = out.str();

        std::istringstream in(data, std::ios::binary);
        // Use direct new to access private constructor within class scope (make_unique won't work)
        std::unique_ptr<Coin> deepcopy(new Coin());
        deepcopy->readDataStream(in);
        return deepcopy;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
    return nullptr;
}

// Streamable
void Coin::writeDataStream(std::ostream& zOut) {
    mCoinID.writeHashToStream(zOut);
    mAddress.writeHashToStream(zOut);
    mAmount.writeDataStream(zOut);
    mTokenID.writeHashToStream(zOut);

    MiniByte::WriteToStream(zOut, mStoreState);

    mMMREntryNumber.writeDataStream(zOut);
    mSpent.writeDataStream(zOut);
    mBlockCreated.writeDataStream(zOut);

    MiniNumber::WriteToStream(zOut, static_cast<int>(mState.size()));
    for (const auto& svptr : mState) {
        svptr->writeDataStream(zOut);
    }

    if (!mToken) {
        MiniByte::WriteToStream(zOut, false);
    } else {
        MiniByte::WriteToStream(zOut, true);
        mToken->writeDataStream(zOut);
    }
}

void Coin::readDataStream(std::istream& zIn) {
    mCoinID        = MiniData::ReadHashFromStream(zIn);
    mAddress       = MiniData::ReadHashFromStream(zIn);
    mAmount        = MiniNumber::ReadFromStream(zIn);
    mTokenID       = MiniData::ReadHashFromStream(zIn);

    mStoreState    = MiniByte::ReadFromStream(zIn).isTrue();

    mMMREntryNumber = MMREntryNumber::ReadFromStream(zIn);
    mSpent          = MiniByte::ReadFromStream(zIn);
    mBlockCreated   = MiniNumber::ReadFromStream(zIn);

    mState.clear();
    int len = MiniNumber::ReadFromStream(zIn).getAsInt();
    mState.reserve(len);
    for (int i = 0; i < len; ++i) {
        mState.emplace_back(StateVariable::ReadFromStream(zIn));
    }

    // Token descriptor
    mToken.reset();
    if (MiniByte::ReadFromStream(zIn).isTrue()) {
        auto tok = Token::ReadFromStream(zIn);
        mToken = std::move(tok);
    }
}

std::unique_ptr<Coin> Coin::ReadFromStream(std::istream& zIn) {
    // Use unique_ptr(new Coin()) to access the private default constructor
    std::unique_ptr<Coin> coin(new Coin());
    coin->readDataStream(zIn);
    return coin;
}