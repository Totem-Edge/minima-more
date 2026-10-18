#include "org/minima/objects/magic.hpp"

#include <sstream>
#include <vector>
#include <algorithm>

#include "org/minima/utils/crypto.hpp"

using org::minima::objects::Magic;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniData;
using org::minima::utils::json::JSONObject;
using boost::multiprecision::cpp_int;

namespace {

// Convert non-negative cpp_int to minimal-length unsigned big-endian byte vector.
// Produces at least one byte (0x00) for value 0.
std::vector<std::uint8_t> cpp_int_to_big_endian(const cpp_int& val_in) {
    if (val_in < 0) {
        throw std::runtime_error("cpp_int_to_big_endian: negative value not supported");
    }
    cpp_int v = val_in;
    std::vector<std::uint8_t> le;
    if (v == 0) {
        le.push_back(0);
    } else {
        while (v > 0) {
            std::uint8_t b = static_cast<std::uint8_t>( (v & 0xFF).convert_to<unsigned int>() );
            le.push_back(b);
            v >>= 8;
        }
    }
    // Reverse for big-endian
    std::vector<std::uint8_t> be(le.rbegin(), le.rend());
    return be;
}

// Parse decimal string to cpp_int using stream extraction
cpp_int cpp_int_from_dec_string(const std::string& dec) {
    std::istringstream iss(dec);
    cpp_int out = 0;
    iss >> out;
    if (!iss) {
        throw std::runtime_error("Failed to parse cpp_int from decimal string");
    }
    return out;
}

} // anonymous namespace

// Static member definitions

// Public static constants
const MiniNumber Magic::MIN_HASHES                 = MiniNumber(10000);
const MiniNumber Magic::DEFAULT_TXPOW_SIZE         = MiniNumber(64 * 1024);
const MiniNumber Magic::DEFAULT_KISSVM_OPERATIONS  = MiniNumber(1024);
const MiniNumber Magic::DEFAULT_TXPOW_TXNS         = MiniNumber(256);

// Private static constants
const MiniNumber Magic::CALC_WEIGHTED              = MiniNumber(16383);
const MiniNumber Magic::CALC_TOTAL                 = MiniNumber(16384);

const MiniNumber Magic::MINMAX_TXPOW_SIZE          = MiniNumber(64 * 1024);
const MiniNumber Magic::MINMAX_KISSVM_OPERATIONS   = MiniNumber(1024);
const MiniNumber Magic::MINMAX_TXPOW_TXNS          = MiniNumber(256);

// BigInteger MIN_TXPOW_VAL = Crypto.MAX_VAL / MIN_HASHES
const cpp_int Magic::MIN_TXPOW_VAL = []() -> cpp_int {
    // Crypto::MAX_VALDEC() is a decimal string for 2^256 - 1
    cpp_int maxval = cpp_int_from_dec_string(org::minima::utils::Crypto::MAX_VALDEC());
    cpp_int denom  = Magic::MIN_HASHES.getAsBigInteger();
    if (denom == 0) {
        throw std::runtime_error("Division by zero computing MIN_TXPOW_VAL");
    }
    return maxval / denom;
}();

// MiniData MIN_TXPOW_WORK from MIN_TXPOW_VAL
const MiniData Magic::MIN_TXPOW_WORK = []() -> MiniData {
    std::vector<std::uint8_t> be = cpp_int_to_big_endian(Magic::MIN_TXPOW_VAL);
    return MiniData(be);
}();

// Constructor
Magic::Magic()
    : mCurrentMaxTxPoWSize(DEFAULT_TXPOW_SIZE)
    , mCurrentMaxKISSVMOps(DEFAULT_KISSVM_OPERATIONS)
    , mCurrentMaxTxnPerBlock(DEFAULT_TXPOW_TXNS)
    , mCurrentMinTxPoWWork(MIN_TXPOW_WORK)
    , mDesiredMaxTxPoWSize(DEFAULT_TXPOW_SIZE)
    , mDesiredMaxKISSVMOps(DEFAULT_KISSVM_OPERATIONS)
    , mDesiredMaxTxnPerBlock(DEFAULT_TXPOW_TXNS)
    , mDesiredMinTxPoWWork(MIN_TXPOW_WORK)
{
}

// JSON
JSONObject Magic::toJSON() const {
    JSONObject magic;
    magic.put("currentmaxtxpowsize", mCurrentMaxTxPoWSize.toString());
    magic.put("currentmaxkissvmops", mCurrentMaxKISSVMOps.toString());
    magic.put("currentmaxtxn",       mCurrentMaxTxnPerBlock.toString());
    magic.put("currentmintxpowwork", mCurrentMinTxPoWWork.to0xString());

    magic.put("desiredmaxtxpowsize", mDesiredMaxTxPoWSize.toString());
    magic.put("desiredmaxkissvmops", mDesiredMaxKISSVMOps.toString());
    magic.put("desiredmaxtxn",       mDesiredMaxTxnPerBlock.toString());
    magic.put("desiredmintxpowwork", mDesiredMinTxPoWWork.to0xString());

    return magic;
}

// Equality of "current" values
bool Magic::checkSame(const Magic& zMagic) const {
    bool w = mCurrentMaxTxPoWSize.isEqual(zMagic.mCurrentMaxTxPoWSize);
    bool x = mCurrentMaxKISSVMOps.isEqual(zMagic.mCurrentMaxKISSVMOps);
    bool y = mCurrentMaxTxnPerBlock.isEqual(zMagic.mCurrentMaxTxnPerBlock);
    bool z = mCurrentMinTxPoWWork.isEqual(zMagic.mCurrentMinTxPoWWork);
    return w && x && y && z;
}

// Getters
MiniNumber Magic::getMaxTxPoWSize() const { return mCurrentMaxTxPoWSize; }
MiniNumber Magic::getMaxKISSOps()   const { return mCurrentMaxKISSVMOps; }
MiniNumber Magic::getMaxNumTxns()   const { return mCurrentMaxTxnPerBlock; }
MiniData   Magic::getMinTxPowWork() const { return mCurrentMinTxPoWWork; }

// Set desired values
void Magic::setDesiredKISSVM(const MiniNumber& zKISSVM) {
    mDesiredMaxKISSVMOps = zKISSVM;
}
void Magic::setDesiredMaxTxPoWSize(const MiniNumber& zMaxSize) {
    mDesiredMaxTxPoWSize = zMaxSize;
}
void Magic::setDesiredMaxTxns(const MiniNumber& zMaxTxn) {
    mDesiredMaxTxnPerBlock = zMaxTxn;
}

// Calculate new current values (weighted average)
Magic Magic::calculateNewCurrent() const {
    Magic ret; // starts with defaults; we'll overwrite the current fields

    // TxPoW Size
    MiniNumber desired = mDesiredMaxTxPoWSize;
    MiniNumber min     = mCurrentMaxTxPoWSize.div(MiniNumber::TWO());
    MiniNumber max     = mCurrentMaxTxPoWSize.mult(MiniNumber::TWO());

    if (desired.isLess(min)) {
        desired = min;
    } else if (desired.isMore(max)) {
        desired = max;
    }

    // Hard minimum limit
    if (desired.isLess(MINMAX_TXPOW_SIZE)) {
        desired = MINMAX_TXPOW_SIZE;
    }

    ret.mCurrentMaxTxPoWSize = mCurrentMaxTxPoWSize.mult(CALC_WEIGHTED).add(desired).div(CALC_TOTAL);

    // KISSVM Ops
    desired = mDesiredMaxKISSVMOps;
    min     = mCurrentMaxKISSVMOps.div(MiniNumber::TWO());
    max     = mCurrentMaxKISSVMOps.mult(MiniNumber::TWO());

    if (desired.isLess(min)) {
        desired = min;
    } else if (desired.isMore(max)) {
        desired = max;
    }

    if (desired.isLess(MINMAX_KISSVM_OPERATIONS)) {
        desired = MINMAX_KISSVM_OPERATIONS;
    }

    ret.mCurrentMaxKISSVMOps = mCurrentMaxKISSVMOps.mult(CALC_WEIGHTED).add(desired).div(CALC_TOTAL);

    // Txns per block
    desired = mDesiredMaxTxnPerBlock;
    min     = mCurrentMaxTxnPerBlock.div(MiniNumber::TWO());
    max     = mCurrentMaxTxnPerBlock.mult(MiniNumber::TWO());

    if (desired.isLess(min)) {
        desired = min;
    } else if (desired.isMore(max)) {
        desired = max;
    }

    if (desired.isLess(MINMAX_TXPOW_TXNS)) {
        desired = MINMAX_TXPOW_TXNS;
    }

    ret.mCurrentMaxTxnPerBlock = mCurrentMaxTxnPerBlock.mult(CALC_WEIGHTED).add(desired).div(CALC_TOTAL);

    // Work (MiniData) uses BigInteger (cpp_int) arithmetic
    cpp_int two = cpp_int(2);
    cpp_int oldval = cpp_int_from_dec_string(mCurrentMinTxPoWWork.getDataValue());
    cpp_int minval = oldval / two;
    cpp_int maxval = oldval * two;

    cpp_int newval = cpp_int_from_dec_string(mDesiredMinTxPoWWork.getDataValue());
    if (newval < minval) {
        newval = minval;
    } else if (newval > maxval) {
        newval = maxval;
    }

    // Hard minimum limit (i.e. ensure newval <= MIN_TXPOW_VAL)
    if (newval > MIN_TXPOW_VAL) {
        newval = MIN_TXPOW_VAL;
    }

    // Weighted average: (oldval * CALC_WEIGHTED + newval) / CALC_TOTAL
    cpp_int calc = oldval * org::minima::objects::base::MiniNumber::ZERO().getAsBigInteger(); // placeholder to get type? but ZERO isn't needed.

    // Correct weighted average using MiniNumber constants' BigInteger values
    cpp_int w = CALC_WEIGHTED.getAsBigInteger();
    cpp_int t = CALC_TOTAL.getAsBigInteger();
    calc = (oldval * w + newval) / t;

    ret.mCurrentMinTxPoWWork = MiniData(cpp_int_to_big_endian(calc));

    return ret;
}

// String
std::string Magic::toString() const {
    return toJSON().toString();
}

// Streamable I/O
void Magic::writeDataStream(std::ostream& out) {
    mCurrentMaxTxPoWSize.writeDataStream(out);
    mCurrentMaxKISSVMOps.writeDataStream(out);
    mCurrentMaxTxnPerBlock.writeDataStream(out);
    mCurrentMinTxPoWWork.writeDataStream(out);

    mDesiredMaxTxPoWSize.writeDataStream(out);
    mDesiredMaxKISSVMOps.writeDataStream(out);
    mDesiredMaxTxnPerBlock.writeDataStream(out);
    mDesiredMinTxPoWWork.writeDataStream(out);
}

void Magic::readDataStream(std::istream& in) {
    mCurrentMaxTxPoWSize   = MiniNumber::ReadFromStream(in);
    mCurrentMaxKISSVMOps   = MiniNumber::ReadFromStream(in);
    mCurrentMaxTxnPerBlock = MiniNumber::ReadFromStream(in);
    mCurrentMinTxPoWWork   = MiniData::ReadFromStream(in);

    mDesiredMaxTxPoWSize   = MiniNumber::ReadFromStream(in);
    mDesiredMaxKISSVMOps   = MiniNumber::ReadFromStream(in);
    mDesiredMaxTxnPerBlock = MiniNumber::ReadFromStream(in);
    mDesiredMinTxPoWWork   = MiniData::ReadFromStream(in);
}

// Static helper
Magic Magic::ReadFromStream(std::istream& in) {
    Magic mag;
    mag.readDataStream(in);
    return mag;
}