#include "org/minima/objects/tx_po_w.hpp"

#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <string_view>

#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/system/params/global_params.hpp"

// Full headers for dependent classes (cycle handling)
#include "org/minima/objects/tx_header.hpp"
#include "org/minima/objects/tx_body.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/magic.hpp"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniByte;
using org::minima::utils::Crypto;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;
using org::minima::system::params::GlobalParams;

// Destructor and move ops (PIMPL/unique_ptr to incomplete types)
TxPoW::~TxPoW() = default;
// TxPoW::TxPoW(TxPoW&&) noexcept = default;
TxPoW& TxPoW::operator=(TxPoW&&) noexcept = default;

TxPoW::TxPoW(TxPoW&& other) noexcept
    : mHeader(std::move(other.mHeader)),
      mBody(std::move(other.mBody)),
      mTxPOWIDStr(std::move(other.mTxPOWIDStr)),
      mTxPOWID(std::move(other.mTxPOWID)),
      mSuperBlock(other.mSuperBlock),
      mTxPoWSize(other.mTxPoWSize),
      mBlockWeight(std::move(other.mBlockWeight)),
      mCheckNumber(other.mCheckNumber),
      mIsTesting(other.mIsTesting),
      mTestBlockNumber(std::move(other.mTestBlockNumber)),
      mTestIsBlock(other.mTestIsBlock),
      mTestIsTxn(other.mTestIsTxn),
      mTestParent(std::move(other.mTestParent)),
      mTestTransactions(std::move(other.mTestTransactions)),
      mIsBlockPOW(other.mIsBlockPOW),
      mIsTxnPOW(other.mIsTxnPOW)
{
    // LOG INFORMATION ABOUT THE MOVE
    // std::string sourceInfo = "UNKNOWN";
    // if (!other.mTxPOWIDStr.empty() && other.mTxPOWIDStr != "MOVED_FROM") {
    //     sourceInfo = "TxPoWID=" + other.mTxPOWIDStr;
    //     try {
    //         sourceInfo += ", Block=" + other.mHeader->mBlockNumber.toString();
    //     } catch (...) {
    //         sourceInfo += " (can't access block number)";
    //     }
    // }
    
    // MinimaLogger::log("MOVE CONSTRUCTOR called - Moving from: " + sourceInfo);
    
    // RECONSTRUCT THE MOVED-FROM OBJECT
    other.mHeader = std::make_unique<TxHeader>();
    other.mBody = std::make_unique<TxBody>();
    other.mTxPOWIDStr = "MOVED_FROM";
    other.mTxPOWID = MiniData::ZERO_TXPOWID();
    other.mBlockWeight = "0";
    other.mIsBlockPOW = false;
    other.mIsTxnPOW = false;
    other.mSuperBlock = 0;
    other.mCheckNumber = 0;
    other.mTxPoWSize = 0;
    
    // std::string myStatus = mHeader ? "VALID" : "NULL";
    // MinimaLogger::log("DEBUG TxPoW move complete - mHeader=" + myStatus + 
    //                  ", source reconstructed");
}

// Constructors
TxPoW::TxPoW()
    : mHeader(std::make_unique<TxHeader>()),
      mBody(std::make_unique<TxBody>()),
      mSuperBlock(0),
      mTxPoWSize(0),
      mBlockWeight("0"),
      mCheckNumber(0),
      mIsTesting(false),
      mIsBlockPOW(false),
      mIsTxnPOW(false)
{
    // std::string headerStatus = mHeader ? "VALID" : "NULL";
    // MinimaLogger::log("DEBUG TxPoW::TxPoW() default constructor - mHeader=" + headerStatus);
}

TxPoW::TxPoW(const std::string& zTxPoWID, int zBlock, int zWeight)
: TxPoW(zTxPoWID, zBlock, zWeight, true, "0x00", false) {}

TxPoW::TxPoW(const std::string& zTxPoWID, int zBlock, int zWeight, bool zIsBlock, const std::string& zParent, bool zIsTransaction) {
    mIsTesting         = true;
    mTxPOWIDStr       = zTxPoWID;
    mTestBlockNumber  = MiniNumber(zBlock);
    mBlockWeight      = std::to_string(zWeight);
    mTestIsBlock      = zIsBlock;
    mTestIsTxn        = zIsTransaction;
    mTestParent       = MiniData(zParent);

    mHeader            = std::make_unique<TxHeader>();
    // In Java test ctor only constructs header; leave body null
    // MinimaLogger::log("DEBUG TxPoW TEST CONSTRUCTOR: ID=" + zTxPoWID + 
    //                  ", weight='" + mBlockWeight + "'");
    mBody.reset();
}

// TEST function
void TxPoW::addTestTransaction(const std::string& zTxPoWID) {
    mTestTransactions.push_back(zTxPoWID);
}

std::vector<std::string> TxPoW::getTransactions() const {
    if (mIsTesting) {
        return mTestTransactions;
    }

    std::vector<std::string> ret;
    if (!hasBody()) {
        return ret;
    }

    for (const auto& blkptr : mBody->mTxPowIDList) {
        if (blkptr) {
            ret.push_back(blkptr->to0xString());
        }
    }

    return ret;
}

TxHeader& TxPoW::getTxHeader() {
    // DEBUG
    // if (!mHeader) {
    //     MinimaLogger::log("FATAL: getTxHeader() called but mHeader is NULL!");
    //     MinimaLogger::log("  TxPoWID: " + mTxPOWIDStr);
    //     MinimaLogger::log("  BlockWeight: " + mBlockWeight);
    //     throw std::runtime_error("TxHeader is null in getTxHeader");
    // }
    // DEBUG END
    // if (!mHeader) throw std::runtime_error("TxHeader is null");
    return *mHeader;
}

const TxHeader& TxPoW::getTxHeader() const {
    // DEBUG
    // if (!mHeader) {
    //     MinimaLogger::log("FATAL: getTxHeader() const called but mHeader is NULL!");
    //     throw std::runtime_error("TxHeader is null in getTxHeader const");
    // }
    // DEBUG END
    // if (!mHeader) throw std::runtime_error("TxHeader is null");
    return *mHeader;
}

MiniData TxPoW::getTxHeaderBodyHash() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null");
    return mHeader->getBodyHash();
}

void TxPoW::setHeaderBodyHash() {
    if (!mBody) throw std::runtime_error("TxBody is null in setHeaderBodyHash");
    if (!mHeader) throw std::runtime_error("TxHeader is null in setHeaderBodyHash");
    mHeader->mTxBodyHash = Crypto::getInstance().hashObject(*mBody);
}

bool TxPoW::isMonotonic() const {
    if (!mBody) throw std::runtime_error("TxBody is null in isMonotonic");
    bool transmon = mBody->mTransaction->isCheckedMonotonic();
    bool burnmon  = true;
    if (!mBody->mBurnTransaction->isEmpty()) {
        burnmon = mBody->mBurnTransaction->isCheckedMonotonic();
    }
    return transmon && burnmon;
}

TxBody* TxPoW::getTxBody() {
    return mBody.get();
}

const TxBody* TxPoW::getTxBody() const {
    return mBody.get();
}

bool TxPoW::hasBody() const {
    return static_cast<bool>(mBody);
}

void TxPoW::clearBody() {
    mBody.reset();
}

void TxPoW::setNonce(const MiniNumber& zNonce) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setNonce");
    mHeader->mNonce = zNonce;
}

MiniNumber TxPoW::getNonce() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getNonce");
    return mHeader->mNonce;
}

MiniData TxPoW::getChainID() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getChainID");
    return mHeader->mChainID;
}

const Magic& TxPoW::getMagic() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getMagic");
    if (!mHeader->mMagic) throw std::runtime_error("Magic is null in TxHeader");
    return *mHeader->mMagic;
}

void TxPoW::setMagic(const Magic& zMagic) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setMagic");
    mHeader->mMagic = std::make_unique<Magic>(zMagic);
}

void TxPoW::setTxDifficulty(const MiniData& zDifficulty) {
    if (!mBody) throw std::runtime_error("TxBody is null in setTxDifficulty");
    mBody->mTxnDifficulty = std::make_unique<MiniData>(zDifficulty);
}

MiniData TxPoW::getTxnDifficulty() const {
    if (!mBody) throw std::runtime_error("TxBody is null in getTxnDifficulty");
    if (!mBody->mTxnDifficulty) throw std::runtime_error("TxnDifficulty is null");
    return *mBody->mTxnDifficulty;
}

Transaction& TxPoW::getTransaction() {
    if (!mBody) throw std::runtime_error("TxBody is null in getTransaction");
    return *mBody->mTransaction;
}

const Transaction& TxPoW::getTransaction() const {
    if (!mBody) throw std::runtime_error("TxBody is null in getTransaction (const)");
    return *mBody->mTransaction;
}

Transaction& TxPoW::getBurnTransaction() {
    if (!mBody) throw std::runtime_error("TxBody is null in getBurnTransaction");
    return *mBody->mBurnTransaction;
}

const Transaction& TxPoW::getBurnTransaction() const {
    if (!mBody) throw std::runtime_error("TxBody is null in getBurnTransaction (const)");
    return *mBody->mBurnTransaction;
}

static std::unique_ptr<Transaction> deepCopyTransaction(const Transaction& zTran) {
    // FIX: Use copy constructor which preserves the cached transaction ID
    // that was used for signing. The serialize/deserialize approach was
    // recalculating the ID which could cause script validation failures.
    return std::make_unique<Transaction>(zTran);
}

static std::unique_ptr<Witness> deepCopyWitness(const Witness& zWit) {
    // FIX: Use copy constructor which preserves all signatures
    return std::make_unique<Witness>(zWit);
}

void TxPoW::setTransaction(const Transaction& zTran) {
    if (!mBody) throw std::runtime_error("TxBody is null in setTransaction");
    mBody->mTransaction = deepCopyTransaction(zTran);
}

void TxPoW::setWitness(const Witness& zWitness) {
    if (!mBody) throw std::runtime_error("TxBody is null in setWitness");
    mBody->mWitness = deepCopyWitness(zWitness);
}

void TxPoW::setBurnTransaction(const Transaction& zTran) {
    if (!mBody) throw std::runtime_error("TxBody is null in setBurnTransaction");
    mBody->mBurnTransaction = deepCopyTransaction(zTran);
}

void TxPoW::setBurnWitness(const Witness& zWitness) {
    if (!mBody) throw std::runtime_error("TxBody is null in setBurnWitness");
    mBody->mBurnWitness = deepCopyWitness(zWitness);
}

Witness& TxPoW::getWitness() {
    if (!mBody) throw std::runtime_error("TxBody is null in getWitness");
    return *mBody->mWitness;
}

const Witness& TxPoW::getWitness() const {
    if (!mBody) throw std::runtime_error("TxBody is null in getWitness (const)");
    return *mBody->mWitness;
}

Witness& TxPoW::getBurnWitness() {
    if (!mBody) throw std::runtime_error("TxBody is null in getBurnWitness");
    return *mBody->mBurnWitness;
}

const Witness& TxPoW::getBurnWitness() const {
    if (!mBody) throw std::runtime_error("TxBody is null in getBurnWitness (const)");
    return *mBody->mBurnWitness;
}

void TxPoW::addBlockTxPOW(const MiniData& zTxPOWID) {
    if (!mBody) throw std::runtime_error("TxBody is null in addBlockTxPOW");
    mBody->mTxPowIDList.push_back(std::make_unique<MiniData>(zTxPOWID));
}

std::vector<MiniData> TxPoW::getBlockTransactions() const {
    if (!hasBody()) {
        return {};
    }
    std::vector<MiniData> res;
    res.reserve(mBody->mTxPowIDList.size());
    for (const auto& ptr : mBody->mTxPowIDList) {
        if (ptr) res.push_back(*ptr);
    }
    return res;
}

MiniData TxPoW::getBlockDifficulty() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getBlockDifficulty");
    return mHeader->mBlockDifficulty;
}

void TxPoW::setBlockDifficulty(const MiniData& zBlockDifficulty) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setBlockDifficulty");
    mHeader->mBlockDifficulty = zBlockDifficulty;
}

MiniData TxPoW::getParentID() const {
    if (mIsTesting) {
        return mTestParent;
    }
    return getSuperParent(0);
}

void TxPoW::setSuperParent(int zLevel, const MiniData& zSuperParent) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setSuperParent");
    mHeader->mSuperParents[zLevel] = zSuperParent;
}

MiniData TxPoW::getSuperParent(int zLevel) const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getSuperParent");
    return mHeader->mSuperParents[zLevel];
}

MiniNumber TxPoW::getBurn() const {
    if (!mBody) throw std::runtime_error("TxBody is null in getBurn");
    // Call underlying non-const methods directly
    return mBody->mTransaction->getBurn().add(mBody->mBurnTransaction->getBurn());
}

void TxPoW::setTimeMilli() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
    setTimeMilli(MiniNumber(static_cast<long long>(ms)));
}

void TxPoW::setTimeMilli(const MiniNumber& zMilli) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setTimeMilli");
    mHeader->mTimeMilli = zMilli;
}

MiniNumber TxPoW::getTimeMilli() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getTimeMilli");
    return mHeader->mTimeMilli;
}

void TxPoW::setBlockNumber(const MiniNumber& zBlockNum) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setBlockNumber");
    mHeader->mBlockNumber = zBlockNum;
}

MiniNumber TxPoW::getBlockNumber() const {
    if (mIsTesting) {
        return mTestBlockNumber;
    }
    if (!mHeader) {
        std::string msg = "ERROR TxPoW::getBlockNumber() - mHeader is NULL!";
        if (!mTxPOWIDStr.empty()) {
            msg += " TxPoWID=" + mTxPOWIDStr;
        } else {
            msg += " TxPoWID=EMPTY";
        }
        MinimaLogger::log(msg);
        throw std::runtime_error("TxHeader is null in getBlockNumber");
    }
    return mHeader->mBlockNumber;
}

MiniData TxPoW::getMMRRoot() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getMMRRoot");
    return mHeader->mMMRRoot;
}

void TxPoW::setMMRRoot(const MiniData& zRoot) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setMMRRoot");
    mHeader->mMMRRoot = zRoot;
}

MiniNumber TxPoW::getMMRTotal() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getMMRTotal");
    return mHeader->mMMRTotal;
}

void TxPoW::setMMRTotal(const MiniNumber& zTotal) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setMMRTotal");
    mHeader->mMMRTotal = zTotal;
}

MiniData TxPoW::getCustomHash() const {
    if (!mHeader) throw std::runtime_error("TxHeader is null in getCustomHash");
    return mHeader->mCustomHash;
}

void TxPoW::setCustomHash(const MiniData& zCustomHash) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in setCustomHash");
    mHeader->mCustomHash = zCustomHash;
}

int TxPoW::getCheckRejectNumber() const {
    return mCheckNumber;
}

void TxPoW::incrementCheckRejectNumber() {
    ++mCheckNumber;
}

JSONObject TxPoW::toJSON() const {
    JSONObject txpow;
    if (mIsTesting) {
        txpow.put("txpowid", mTxPOWIDStr);
        txpow.put("parentid", mTestParent.to0xString());
        txpow.put("blocknumber", mTestBlockNumber.toString());
        
        // DEBUG: Log weight before serialization
        // MinimaLogger::log("DEBUG toJSON: TxPoWID=" + mTxPOWIDStr + ", mBlockWeight='" + mBlockWeight + "'");

        // FIX: mBlockWeight is already a string - ensure no scientific notation
        std::string weightStr = mBlockWeight;

        // Ensure weight is never empty
        if (weightStr.empty()) {
            MinimaLogger::log("WARNING toJSON: Empty weight for TxPoWID=" + mTxPOWIDStr + ", defaulting to 0");
            weightStr = "0";
        }


        // Check if weight contains scientific notation (e/E)
        if (weightStr.find('e') != std::string::npos || weightStr.find('E') != std::string::npos) {
            MinimaLogger::log("WARNING toJSON: Weight has scientific notation, converting...");
            try {
                MiniNumber weightNum(weightStr);
                weightStr = weightNum.toString();
                // MinimaLogger::log("DEBUG toJSON: Converted from '" + mBlockWeight + "' to '" + weightStr + "'");
            } catch (const std::exception& e) {
                MinimaLogger::log("ERROR toJSON: Failed to convert weight: " + std::string(e.what()));
                weightStr = "0";
            }
        }
        txpow.put("weight", weightStr);

        txpow.put("isblock", mTestIsBlock);
        txpow.put("istxn", mTestIsTxn);
    } else {
        txpow.put("txpowid", mTxPOWID.toString());
        txpow.put("isblock", mIsBlockPOW);
        txpow.put("istransaction", mIsTxnPOW);
        txpow.put("superblock", mSuperBlock);
        txpow.put("size", mTxPoWSize);
        // Compute burn directly to avoid const issues
        if (hasBody()) {
            txpow.put("burn", mBody->mTransaction->getBurn().add(mBody->mBurnTransaction->getBurn()).toString());
        } else {
            txpow.put("burn", std::string("0"));
        }

        txpow.put("header", mHeader ? mHeader->toJSON() : JSONObject());

        txpow.put("hasbody", hasBody());
        if (hasBody()) {
            txpow.put("body", mBody->toJSON());
        } else {
            txpow.put("body", std::string("null"));
        }
    }
    return txpow;
}

std::string TxPoW::toString() const {
    return toJSON().toString();
}

void TxPoW::writeDataStream(std::ostream& out) {
    if (!mHeader) throw std::runtime_error("TxHeader is null in writeDataStream");
    mHeader->writeDataStream(out);
    if (!mBody) {
        MiniByte boolVal(false);
        boolVal.writeDataStream(out);
    } else {
        MiniByte boolVal(true);
        boolVal.writeDataStream(out);
        mBody->writeDataStream(out);
    }
}

void TxPoW::readDataStream(std::istream& in) {
    // MinimaLogger::log("DEBUG TxPoW::readDataStream START");

    if (!mHeader) {
        mHeader = std::make_unique<TxHeader>();
    }

    // try {
    //     mHeader->readDataStream(in);
    // } catch (const std::exception& e) {
    //     std::cout << "DEBUG: TxPoW - TxHeader read FAILED at pos " 
    //               << in.tellg() << ": " << e.what() << std::endl;
    //     // Create fresh default header instead of using corrupted one
    //     mHeader = std::make_unique<TxHeader>();
    // }

    mHeader->readDataStream(in);

    // CRITICAL DEBUG - Check if mHeader is valid after read
    // if (!mHeader) {
    //     MinimaLogger::log("FATAL: mHeader is NULL immediately after readDataStream!");
    //     throw std::runtime_error("TxHeader null after TxHeader::readDataStream");
    // }
    
    // // Verify we can access it - Use mBlockNumber directly (public field)
    // try {
    //     std::string blockNum = mHeader->mBlockNumber.toString();
    //     MinimaLogger::log("DEBUG: TxPoW read - Block " + blockNum + " mHeader valid");
    // } catch (const std::exception& e) {
    //     MinimaLogger::log("FATAL: Cannot access mHeader->mBlockNumber: " + std::string(e.what()));
    //     throw;
    // }
    // END DEBUG

    MiniByte hasBody = MiniByte::ReadFromStream(in);
    if (hasBody.isTrue()) {
        if (!mBody) {
            mBody = std::make_unique<TxBody>();
        }
        mBody->readDataStream(in);
    } else {
        mBody.reset();
    }

    // Calculate derived data
    calculateTXPOWID();

    // DEBUG VERIFY HEADER STILL VALID AFTER calculateTXPOWID
    // if (!mHeader) {
    //     MinimaLogger::log("FATAL: mHeader became NULL after calculateTXPOWID!");
    //     throw std::runtime_error("TxHeader null after calculateTXPOWID");
    // }
    // DEBUG END

    // SAFETY: Ensure mBlockWeight is never empty
    if (mBlockWeight.empty()) {
        mBlockWeight = "0";
        // MinimaLogger::log("DEBUG: TxPoW readDataStream - empty weight defaulted to '0', ID=" + mTxPOWIDStr);
    }

    // MinimaLogger::log(std::string("DEBUG TxPoW::readDataStream END - mHeader is ") + (mHeader ? "VALID" : "NULL"));
}

std::unique_ptr<TxPoW> TxPoW::ReadFromStream(std::istream& in) {
    auto txp = std::make_unique<TxPoW>();
    txp->readDataStream(in);
    return txp;
}

std::unique_ptr<TxPoW> TxPoW::deepCopy() const {
    try {
        auto copy = std::make_unique<TxPoW>();
        copy->copyFrom(*this);
        return copy;

    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

void TxPoW::copyFrom(const TxPoW& zOther) {
    mHeader = zOther.mHeader ? std::make_unique<TxHeader>(*zOther.mHeader) : nullptr;
    mBody   = zOther.mBody   ? std::make_unique<TxBody>(*zOther.mBody)     : nullptr;

    mTxPOWIDStr = zOther.mTxPOWIDStr;
    mTxPOWID    = zOther.mTxPOWID;
    mSuperBlock = zOther.mSuperBlock;
    mTxPoWSize  = zOther.mTxPoWSize;
    mBlockWeight = zOther.mBlockWeight;
    mCheckNumber = zOther.mCheckNumber;

    mIsTesting       = zOther.mIsTesting;
    mTestBlockNumber = zOther.mTestBlockNumber;
    mTestIsBlock     = zOther.mTestIsBlock;
    mTestIsTxn       = zOther.mTestIsTxn;
    mTestParent      = zOther.mTestParent;
    mTestTransactions = zOther.mTestTransactions;

    mIsBlockPOW = zOther.mIsBlockPOW;
    mIsTxnPOW   = zOther.mIsTxnPOW;
}

std::unique_ptr<TxPoW> TxPoW::convertMiniDataVersion(const MiniData& zTxpData) {
    try {
        const std::vector<uint8_t>& b = zTxpData.getBytes();
        std::string_view s(reinterpret_cast<const char*>(b.data()), b.size());
        std::istringstream iss(std::string(s), std::ios::binary);

        return TxPoW::ReadFromStream(iss);  // Returns unique_ptr directly
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
    return nullptr;
}

void TxPoW::calculateTransactionID() {
    if (!mBody) throw std::runtime_error("TxBody is null in calculateTransactionID");
    mBody->mTransaction->calculateTransactionID();
    mBody->mBurnTransaction->calculateTransactionID();
}

std::string TxPoW::getTxPoWID() const {
    return mTxPOWIDStr;
}

MiniData TxPoW::getTxPoWIDData() const {
    return mTxPOWID;
}

int TxPoW::getSuperLevel() const {
    return mSuperBlock;
}

bool TxPoW::isBlock() const {
    if (mIsTesting) {
        return mTestIsBlock;
    }
    return mIsBlockPOW;
}

std::string TxPoW::getWeight() const {
    return mBlockWeight;
}

bool TxPoW::isTransaction() const {
    if (mIsTesting) {
        return mTestIsTxn;
    }
    if (!hasBody()) {
        return false;
    }
    return !getTransaction().isEmpty() || !getBurnTransaction().isEmpty();
}

std::int64_t TxPoW::getSizeinBytes() const {
    return mTxPoWSize;
}

std::int64_t TxPoW::getSizeinBytesWithoutBlockTxns() const {
    std::int64_t txns = static_cast<std::int64_t>(32) * static_cast<std::int64_t>(getBlockTransactions().size());
    return mTxPoWSize - txns;
}

void TxPoW::calculateTXPOWID() {
    // The TXPOW ID
    mTxPOWID    = Crypto::getInstance().hashObject(*mHeader);
    mTxPOWIDStr = mTxPOWID.to0xString();

    // Valid Block
    mIsBlockPOW = mTxPOWID.isLess(getBlockDifficulty());
    mBlockWeight = "0";
    if (mIsBlockPOW) {
        try {
            // Use cpp_dec_float_100 for the division with limited precision
            using boost::multiprecision::cpp_dec_float_100;
            
            cpp_dec_float_100 maxval_dec(Crypto::MAX_VALDEC());
            cpp_dec_float_100 blk_dec(getBlockDifficulty().getDataValue());
            
            // MinimaLogger::log(
            //     "DEBUG: calculateTXPOWID: Block " + getBlockNumber().toString() +
            //     ", blockdifficulty=" + getBlockDifficulty().getDataValue());
            
            if (blk_dec != 0) {
                // Perform division with floating point (limited precision)
                cpp_dec_float_100 w_dec = maxval_dec / blk_dec;
                
                // Convert to string with 7 significant figures (DECIMAL32 precision)
                std::ostringstream oss;
                oss << std::setprecision(7) << w_dec;
                mBlockWeight = oss.str();
                
                // MinimaLogger::log(
                //     "DEBUG: calculateTXPOWID: Calculated mBlockWeight=" + mBlockWeight);
            } else {
                mBlockWeight = "0";
                // MinimaLogger::log(
                //     "DEBUG: calculateTXPOWID: Block difficulty is zero, weight=0");
            }
            
        } catch (const std::exception& e) {
            // MinimaLogger::log(
            //     "DEBUG: calculateTXPOWID: Weight calculation failed: " + std::string(e.what()));
            mBlockWeight = "0";
        }
    }

    // The Transaction ID
    mIsTxnPOW = false;
    if (hasBody()) {
        // Compute transaction IDs
        calculateTransactionID();

        // Valid Transaction
        if (mTxPOWID.isLess(getTxnDifficulty()) && !getTransaction().isEmpty()) {
            mIsTxnPOW = true;
        }
    }

    // What Super Level are we..
    mSuperBlock = TxPoW::getSuperLevel(getBlockDifficulty(), mTxPOWID);
    if (mSuperBlock >= GlobalParams::MINIMA_CASCADE_LEVELS) {
        mSuperBlock = GlobalParams::MINIMA_CASCADE_LEVELS - 1;
    }

    // Determine size by serializing to bytes
    try {
        std::ostringstream oss(std::ios::binary);
        writeDataStream(oss);
        mTxPoWSize = static_cast<std::int64_t>(oss.str().size());
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }

    // SAFETY: Ensure mBlockWeight is never empty
    if (mBlockWeight.empty()) {
        mBlockWeight = "0";
        // MinimaLogger::log("DEBUG TxPoW::calculateTXPOWID - empty weight defaulted to 0, ID=" + mTxPOWIDStr);
    }
}

static boost::multiprecision::cpp_int parse_decimal_cpp_int(const std::string& s) {
    using boost::multiprecision::cpp_int;
    std::istringstream iss(s);
    cpp_int v(0);
    iss >> v;
    return v;
}

int TxPoW::getSuperLevel(const MiniData& zBlockDifficulty, const MiniData& zTxPoWID) {
    using boost::multiprecision::cpp_int;
    using boost::multiprecision::msb;

    // quot = zBlockDifficulty.getDataValue() / zTxPoWID.getDataValue();
    cpp_int blk = parse_decimal_cpp_int(zBlockDifficulty.getDataValue());
    cpp_int tid = parse_decimal_cpp_int(zTxPoWID.getDataValue());

    if (tid == 0) {
        // Java BigInteger.divide by zero would throw; conservative value
        return -1;
    }

    cpp_int quot = blk / tid;
    if (quot == 0) {
        return -1; // bitLength() == 0 => -1
    }
    // bitLength()-1 equals msb (0-based index of highest bit)
    return static_cast<int>(msb(quot));
}

} // namespace objects
} // namespace minima
} // namespace org