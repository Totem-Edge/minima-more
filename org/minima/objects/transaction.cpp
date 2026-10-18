#include "org/minima/objects/transaction.hpp"

#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <stdexcept>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Full headers for forward-declared project types
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/token.hpp"

#ifdef _WIN32
// No OS-specific behavior needed currently.
#endif

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::Crypto;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

// Static helper
bool Transaction::stateVarPortLess(
    const std::unique_ptr<StateVariable>& a,
    const std::unique_ptr<StateVariable>& b) {
    return a->getPort() < b->getPort();
}

// Constructor
Transaction::Transaction()
    : mLinkHash(std::make_unique<MiniData>(MiniData::ZERO_TXPOWID())),
      mTransactionID(std::make_unique<MiniData>(MiniData::ZERO_TXPOWID())) {}

// Destructor and move operations (required for unique_ptr to incomplete types)
Transaction::~Transaction() = default;
Transaction::Transaction(Transaction&&) noexcept = default;
Transaction& Transaction::operator=(Transaction&&) noexcept = default;

Transaction::Transaction(const Transaction& zOther)
    : mHaveCheckedMonotonic(zOther.mHaveCheckedMonotonic),
      mIsMonotonic(zOther.mIsMonotonic),
      mIsValid(zOther.mIsValid)
{
    // Manually deep-copy unique_ptr members
    if (zOther.mLinkHash) {
        mLinkHash = std::make_unique<org::minima::objects::base::MiniData>(*zOther.mLinkHash);
    }
    if (zOther.mTransactionID) {
        mTransactionID = std::make_unique<org::minima::objects::base::MiniData>(*zOther.mTransactionID);
    }

    // Manually deep-copy vectors of ptrs
    mInputs.reserve(zOther.mInputs.size());
    for (const auto& in : zOther.mInputs) {
        // deepCopy() returns a unique_ptr. Construct a new shared_ptr from it.
        // This preserves the deep-copy behavior of the original C++ code.
        mInputs.push_back(std::shared_ptr<Coin>(in ? in->deepCopy() : nullptr));
    }

    mOutputs.reserve(zOther.mOutputs.size());
    for (const auto& out : zOther.mOutputs) {
        mOutputs.push_back(out ? out->deepCopy() : nullptr); // Assumes Coin::deepCopy exists
    }

    mState.reserve(zOther.mState.size());
    for (const auto& sv : zOther.mState) {
        // Assumes StateVariable is copyable (which it is)
        mState.push_back(sv ? std::make_unique<StateVariable>(*sv) : nullptr);
    }
}

// Inputs / Outputs management
void Transaction::addInput(std::shared_ptr<Coin> zCoin) {
    mInputs.emplace_back(std::move(zCoin));
}

void Transaction::addOutput(std::unique_ptr<Coin> zCoin) {
    mOutputs.emplace_back(std::move(zCoin));
}

bool Transaction::isEmpty() const {
    return mInputs.empty() && mOutputs.empty();
}

std::vector<std::shared_ptr<Coin>>& Transaction::getAllInputs() { return mInputs; }
const std::vector<std::shared_ptr<Coin>>& Transaction::getAllInputs() const { return mInputs; }

std::vector<std::unique_ptr<Coin>>& Transaction::getAllOutputs() { return mOutputs; }
const std::vector<std::unique_ptr<Coin>>& Transaction::getAllOutputs() const { return mOutputs; }

// Monotonic flags
bool Transaction::isCheckedMonotonic() const {
    return mHaveCheckedMonotonic && mIsMonotonic;
}

void Transaction::clearIsMonotonic() {
    mHaveCheckedMonotonic = false;
}

// Summations
MiniNumber Transaction::sumInputs() {
    MiniNumber tot = MiniNumber::ZERO();
    for (const auto& cc : mInputs) {
        tot = tot.add(cc->getAmount());
    }
    return tot;
}

MiniNumber Transaction::sumInputs(const MiniData& zTokenID) {
    MiniNumber tot = MiniNumber::ZERO();
    for (const auto& cc : mInputs) {
        if (cc->getTokenID().isEqual(zTokenID)) {
            tot = tot.add(cc->getAmount());
        }
    }
    return tot;
}

MiniNumber Transaction::sumOutputs() {
    MiniNumber tot = MiniNumber::ZERO();
    for (const auto& cc : mOutputs) {
        tot = tot.add(cc->getAmount());
    }
    return tot;
}

MiniNumber Transaction::sumOutputs(const MiniData& zTokenID) {
    MiniNumber tot = MiniNumber::ZERO();
    for (const auto& cc : mOutputs) {
        if (cc->getTokenID().isEqual(zTokenID)) {
            tot = tot.add(cc->getAmount());
        }
    }
    return tot;
}

// Validation
bool Transaction::checkValid() {
    int ins = static_cast<int>(mInputs.size());
    if (ins < 1) {
        return false;
    }

    // Totals
    MiniNumber totalin = MiniNumber::ZERO();
    MiniNumber totalout = MiniNumber::ZERO();
    for (const auto& cc : mInputs) {
        totalin = totalin.add(cc->getAmount());
    }
    for (const auto& cc : mOutputs) {
        totalout = totalout.add(cc->getAmount());
    }
    if (totalout.isMore(totalin)) {
        MinimaLogger::log(std::string("Transaction error : Inputs LESS than Outputs ")
                          + totalin.toString() + "/" + totalout.toString());
        return false;
    }

    // Minima value validation
    for (const auto& cc : mInputs) {
        if (!cc->getAmount().isValidMinimaValue()) {
            MinimaLogger::log(std::string("Transaction error : Input is invalid Minima Amount ")
                              + cc->getAmount().toString());
            return false;
        }
    }
    for (const auto& cc : mOutputs) {
        if (!cc->getAmount().isValidMinimaValue()) {
            MinimaLogger::log(std::string("Transaction error : Output is invalid Minima Amount ")
                              + cc->getAmount().toString());
            return false;
        }
    }

    // Collect output tokens (normalizing CREATE to MINIMA)
    std::vector<std::string> tokens;
    tokens.reserve(mOutputs.size());
    for (const auto& cc : mOutputs) {
        MiniData tokenhash = cc->getTokenID();
        if (tokenhash.isEqual(Token::TOKENID_CREATE)) {
            tokenhash = Token::TOKENID_MINIMA;
        }
        std::string tok = tokenhash.to0xString();
        if (std::find(tokens.begin(), tokens.end(), tok) == tokens.end()) {
            tokens.emplace_back(tok);
        }
    }

    // Output amounts per token
    std::unordered_map<std::string, MiniNumber> outamounts;
    for (const auto& token : tokens) {
        MiniData tokmd(token);
        outamounts[token] = sumOutputs(tokmd);
    }

    // Check there are enough inputs for each token
    for (const auto& kv : outamounts) {
        const std::string& tok = kv.first;
        const MiniNumber& outamt = kv.second;

        MiniData tokmd(tok);
        MiniNumber inamt = sumInputs(tokmd);

        if (inamt.isLess(outamt)) {
            MinimaLogger::log("Transaction error : Inputs LESS than Outputs");
            return false;
        }
    }

    // Unique input CoinID check
    if (ins > 1) {
        for (int i = 0; i < ins; ++i) {
            for (int j = i + 1; j < ins; ++j) {
                const Coin* input1 = mInputs[i].get();
                const Coin* input2 = mInputs[j].get();
                if (input1->getCoinID().isEqual(input2->getCoinID())) {
                    return false;
                }
            }
        }
    }

    return true;
}

// Burn amount
MiniNumber Transaction::getBurn() {
    MiniNumber totalin = MiniNumber::ZERO();
    MiniNumber totalout = MiniNumber::ZERO();

    for (const auto& cc : mInputs) {
        totalin = totalin.add(cc->getAmount());
    }
    for (const auto& cc : mOutputs) {
        totalout = totalout.add(cc->getAmount());
    }
    return totalin.sub(totalout);
}

// State variables
void Transaction::addStateVariable(std::unique_ptr<StateVariable> zValue) {
    // Remove existing with same port
    removeStateVariable(zValue->getPort());

    // Add new
    mState.emplace_back(std::move(zValue));

    // Sort by port
    std::sort(mState.begin(), mState.end(), &Transaction::stateVarPortLess);
}

void Transaction::removeStateVariable(int zPort) {
    // First check if any element matches
    bool found = false;
    for (const auto& sv : mState) {
        if (sv->getPort() == zPort) {
            found = true;
            break;
        }
    }
    if (!found) {
        return; // leave mState unchanged
    }

    // Build new list without the matching port
    std::vector<std::unique_ptr<StateVariable>> newvars;
    newvars.reserve(mState.size());
    for (auto& sv : mState) {
        if (sv->getPort() != zPort) {
            newvars.emplace_back(std::move(sv));
        }
    }
    mState = std::move(newvars);

    // Sort by port
    std::sort(mState.begin(), mState.end(), &Transaction::stateVarPortLess);
}

StateVariable* Transaction::getStateValue(int zPort) {
    for (auto& sv : mState) {
        if (sv->getPort() == zPort) {
            return sv.get();
        }
    }
    return nullptr;
}

const StateVariable* Transaction::getStateValue(int zPort) const {
    for (const auto& sv : mState) {
        if (sv->getPort() == zPort) {
            return sv.get();
        }
    }
    return nullptr;
}

bool Transaction::stateExists(int zStateNum) const {
    return getStateValue(zStateNum) != nullptr;
}

void Transaction::clearState() {
    mState.clear();
}

std::vector<std::unique_ptr<StateVariable>>& Transaction::getCompleteState() { return mState; }
const std::vector<std::unique_ptr<StateVariable>>& Transaction::getCompleteState() const { return mState; }

// Link hash
MiniData Transaction::getLinkHash() const {
    return *mLinkHash;
}

void Transaction::setLinkHash(const MiniData& zLinkHash) {
    mLinkHash = std::make_unique<MiniData>(zLinkHash);
}

// Transaction ID
void Transaction::calculateTransactionID() {
    MiniData tid = Crypto::getInstance().hashObject(*this);
    mTransactionID = std::make_unique<MiniData>(tid);
    // In Java null would throw; here hashObject returns a value, so no null.
}

MiniData Transaction::getTransactionID() const {
    return *mTransactionID;
}

// Calculate coin ID for an output
MiniData Transaction::calculateCoinID(const MiniData& zBaseCoinID, int zOutput) {
    MiniData base = zBaseCoinID; // non-const for Crypto API
    MiniNumber outnum(zOutput);
    return Crypto::getInstance().hashObjects(base, outnum);
}

// JSON
std::string Transaction::toString() {
    return toJSON().toString();
}

JSONObject Transaction::toJSON() const {
    JSONObject ret;

    // Inputs
    JSONArray ins;
    for (const auto& in : mInputs) {
        ins.add(in->toJSON());
    }
    ret.put("inputs", ins);

    // Outputs
    JSONArray outs;
    for (const auto& out : mOutputs) {
        outs.add(out->toJSON());
    }
    ret.put("outputs", outs);

    // State
    JSONArray states;
    for (const auto& sv : mState) {
        states.add(sv->toJSON());
    }
    ret.put("state", states);

    ret.put("linkhash", mLinkHash->to0xString());

    // calculateTransactionID();
    ret.put("transactionid", mTransactionID->to0xString());

    return ret;
}

// State size calculation
long long Transaction::calculateStateSize() const {
    long long ret = 1000000;
    try {
        std::ostringstream oss(std::ios::binary);

        // How many state variables
        MiniNumber statelen(static_cast<int>(mState.size()));
        statelen.writeDataStream(oss);
        for (const auto& sv : mState) {
            sv->writeDataStream(oss);
        }
        std::string data = oss.str();
        ret = static_cast<long long>(data.size());
    } catch (const std::exception& exc) {
        MinimaLogger::log("Calcualte state size error!");
        MinimaLogger::log(exc);
    }
    return ret;
}

// Streamable
void Transaction::writeDataStream(std::ostream& out) {
    // Inputs
    MiniNumber ins(static_cast<int>(mInputs.size()));
    ins.writeDataStream(out);
    for (const auto& coin : mInputs) {
        coin->writeDataStream(out);
    }

    // Outputs
    MiniNumber outs(static_cast<int>(mOutputs.size()));
    outs.writeDataStream(out);
    for (const auto& coin : mOutputs) {
        coin->writeDataStream(out);
    }

    // State variables
    MiniNumber statelen(static_cast<int>(mState.size()));
    statelen.writeDataStream(out);
    for (const auto& sv : mState) {
        sv->writeDataStream(out);
    }

    // Link hash
    mLinkHash->writeHashToStream(out);
}

void Transaction::readDataStream(std::istream& in) {
    mInputs.clear();
    mOutputs.clear();
    mState.clear();

    // Inputs
    MiniNumber ins = MiniNumber::ReadFromStream(in);
    int len = ins.getAsInt();
    mInputs.reserve(len);
    for (int i = 0; i < len; ++i) {
        mInputs.emplace_back(Coin::ReadFromStream(in));
    }

    // Outputs
    MiniNumber outs = MiniNumber::ReadFromStream(in);
    len = outs.getAsInt();
    mOutputs.reserve(len);
    for (int i = 0; i < len; ++i) {
        mOutputs.emplace_back(Coin::ReadFromStream(in));
    }

    // State variables
    MiniNumber states = MiniNumber::ReadFromStream(in);
    len = states.getAsInt();
    mState.reserve(len);
    for (int i = 0; i < len; ++i) {
        mState.emplace_back(StateVariable::ReadFromStream(in));
    }

    *mLinkHash = MiniData::ReadHashFromStream(in);

    calculateTransactionID();
}

} // namespace objects
} // namespace minima
} // namespace org