#include "org/minima/objects/tx_body.hpp"

#include <utility>
#include <stdexcept>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"

namespace org {
namespace minima {
namespace objects {

// Destructor and move operations definitions (Pitfall 1)
TxBody::~TxBody() = default;
TxBody::TxBody(TxBody&&) noexcept = default;
TxBody& TxBody::operator=(TxBody&&) noexcept = default;

TxBody::TxBody()
    : mPRNG(std::make_unique<org::minima::objects::base::MiniData>(
          org::minima::objects::base::MiniData::getRandomData(32))),
      mTxnDifficulty(std::make_unique<org::minima::objects::base::MiniData>(
          org::minima::utils::Crypto::MAX_HASH())),
      mTransaction(std::make_unique<org::minima::objects::Transaction>()),
      mWitness(std::make_unique<org::minima::objects::Witness>()),
      mBurnTransaction(std::make_unique<org::minima::objects::Transaction>()),
      mBurnWitness(std::make_unique<org::minima::objects::Witness>()),
      mTxPowIDList() {
    // Vector is default-initialized empty, matching Java constructor
}

// ADD THIS FUNCTION TO tx_body.cpp
TxBody::TxBody(const TxBody& zOther)
{
    // Manually deep-copy all unique_ptr members
    // This assumes all these classes are copyable (which we've been fixing)
    if (zOther.mPRNG) {
        mPRNG = std::make_unique<org::minima::objects::base::MiniData>(*zOther.mPRNG);
    }
    if (zOther.mTxnDifficulty) {
        mTxnDifficulty = std::make_unique<org::minima::objects::base::MiniData>(*zOther.mTxnDifficulty);
    }
    if (zOther.mTransaction) {
        mTransaction = std::make_unique<Transaction>(*zOther.mTransaction);
    }
    if (zOther.mWitness) {
        mWitness = std::make_unique<Witness>(*zOther.mWitness);
    }
    if (zOther.mBurnTransaction) {
        mBurnTransaction = std::make_unique<Transaction>(*zOther.mBurnTransaction);
    }
    if (zOther.mBurnWitness) {
        mBurnWitness = std::make_unique<Witness>(*zOther.mBurnWitness);
    }

    // Manually deep-copy the vector of unique_ptrs
    mTxPowIDList.reserve(zOther.mTxPowIDList.size());
    for (const auto& txpid : zOther.mTxPowIDList) {
        if (txpid) {
            mTxPowIDList.push_back(std::make_unique<org::minima::objects::base::MiniData>(*txpid));
        } else {
            mTxPowIDList.push_back(nullptr);
        }
    }
}

void TxBody::resetRandomPRNG() {
    mPRNG = std::make_unique<org::minima::objects::base::MiniData>(
        org::minima::objects::base::MiniData::getRandomData(32));
}

org::minima::utils::json::JSONObject TxBody::toJSON() const {
    using org::minima::objects::base::MiniData;
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    JSONObject txpow;

    // prng
    txpow.put("prng", mPRNG ? mPRNG->to0xString() : std::string(""));

    // txndiff
    txpow.put("txndiff", mTxnDifficulty ? mTxnDifficulty->to0xString() : std::string(""));

    // txn and witness
    if (mTransaction) {
        txpow.put("txn", mTransaction->toJSON());
    }
    if (mWitness) {
        txpow.put("witness", mWitness->toJSON());
    }

    // burntxn and burnwitness (may be empty)
    if (mBurnTransaction) {
        txpow.put("burntxn", mBurnTransaction->toJSON());
    }
    if (mBurnWitness) {
        txpow.put("burnwitness", mBurnWitness->toJSON());
    }

    // txnlist as a JSON array of hex strings
    JSONArray txns;
    for (const auto& txpid : mTxPowIDList) {
        if (txpid) {
            txns.add(txpid->to0xString());
        } else {
            txns.add(std::string("")); // conservative fallback if null
        }
    }
    txpow.put("txnlist", txns);

    return txpow;
}

void TxBody::writeDataStream(std::ostream& out) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;

    if (!mPRNG || !mTxnDifficulty || !mTransaction || !mWitness || !mBurnTransaction || !mBurnWitness) {
        throw std::runtime_error("TxBody::writeDataStream: Null member encountered");
    }

    // mPRNG as HASH
    mPRNG->writeHashToStream(out);

    // Difficulty and all embedded objects
    mTxnDifficulty->writeDataStream(out);
    mTransaction->writeDataStream(out);
    mWitness->writeDataStream(out);
    mBurnTransaction->writeDataStream(out);
    mBurnWitness->writeDataStream(out);

    // Write TXPOW list length as MiniNumber created from string
    int len = static_cast<int>(mTxPowIDList.size());
    org::minima::objects::base::MiniNumber ramlen(std::to_string(len));
    ramlen.writeDataStream(out);

    // Write each TXPOWID as HASH
    for (const auto& txpowid : mTxPowIDList) {
        if (!txpowid) {
            throw std::runtime_error("TxBody::writeDataStream: Null TXPOWID encountered");
        }
        txpowid->writeHashToStream(out);
    }
}

void TxBody::readDataStream(std::istream& in) {
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;

    // mPRNG from HASH
    mPRNG = std::make_unique<MiniData>(MiniData::ReadHashFromStream(in));

    // Difficulty and embedded objects
    mTxnDifficulty = std::make_unique<MiniData>(MiniData::ReadFromStream(in));

    // Ensure nested objects exist
    if (!mTransaction) {
        mTransaction = std::make_unique<org::minima::objects::Transaction>();
    }
    if (!mWitness) {
        mWitness = std::make_unique<org::minima::objects::Witness>();
    }
    if (!mBurnTransaction) {
        mBurnTransaction = std::make_unique<org::minima::objects::Transaction>();
    }
    if (!mBurnWitness) {
        mBurnWitness = std::make_unique<org::minima::objects::Witness>();
    }

    mTransaction->readDataStream(in);
    mWitness->readDataStream(in);
    mBurnTransaction->readDataStream(in);
    mBurnWitness->readDataStream(in);

    // Read TXPOW list
    mTxPowIDList.clear();
    MiniNumber ramlen = MiniNumber::ReadFromStream(in);
    int len = ramlen.getAsInt();
    if (len < 0) {
        throw std::runtime_error("TxBody::readDataStream: Negative txn list length");
    }
    mTxPowIDList.reserve(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        MiniData id = MiniData::ReadHashFromStream(in);
        mTxPowIDList.emplace_back(std::make_unique<MiniData>(std::move(id)));
    }
}

} // namespace objects
} // namespace minima
} // namespace org