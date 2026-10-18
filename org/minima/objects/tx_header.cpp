#include "org/minima/objects/tx_header.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "org/minima/objects/magic.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniByte;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::params::GlobalParams;
using org::minima::utils::Crypto;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

// Static members
MiniData TxHeader::MAIN_NET("0x00");
MiniData TxHeader::TEST_NET("0x01");

// Helper: current time millis since epoch
static long long get_current_time_millis() {
    using namespace std::chrono;
    auto now = time_point_cast<milliseconds>(system_clock::now());
    return now.time_since_epoch().count();
}

// Helper: format date string similar to Java's new Date(ms).toString()
static std::string format_date_millis(long long ms) {
    std::time_t tt = static_cast<std::time_t>(ms / 1000);
    std::tm tmval{};
#ifdef _WIN32
    localtime_s(&tmval, &tt);
#else
    localtime_r(&tt, &tmval);
#endif
    std::ostringstream oss;
    // Close approximation: "Wed Oct 25 15:45:12 GMT 2025" (timezone may vary)
    oss << std::put_time(&tmval, "%a %b %d %H:%M:%S %Z %Y");
    return oss.str();
}

// Special members (PIMPL fix for unique_ptr<Magic>)
TxHeader::~TxHeader() = default;
TxHeader::TxHeader(TxHeader&&) noexcept = default;
TxHeader& TxHeader::operator=(TxHeader&&) noexcept = default;

TxHeader::TxHeader()
    : mNonce(0)
    , mChainID(MAIN_NET)
    , mTimeMilli(get_current_time_millis())
    , mBlockNumber(0)
    , mBlockDifficulty(Crypto::MAX_HASH())
    , mSuperParents()
    , mMagic(std::make_unique<org::minima::objects::Magic>())
    , mMMRRoot("0x00")
    , mMMRTotal(MiniNumber::ZERO())
    , mCustomHash(MiniData::ZERO_TXPOWID())
    , mTxBodyHash("0x00") {
    // Initialize super parents based on cascade levels
    int levels = GlobalParams::MINIMA_CASCADE_LEVELS;
    if (levels < 0) levels = 0;
    mSuperParents.resize(static_cast<std::size_t>(levels));
    for (int i = 0; i < levels; ++i) {
        mSuperParents[static_cast<std::size_t>(i)] = MiniData(); // default
    }
}

TxHeader::TxHeader(const TxHeader& zOther)
    : mNonce(zOther.mNonce),
      mChainID(zOther.mChainID),
      mTimeMilli(zOther.mTimeMilli),
      mBlockNumber(zOther.mBlockNumber),
      mBlockDifficulty(zOther.mBlockDifficulty),
      mSuperParents(zOther.mSuperParents),
      mMMRRoot(zOther.mMMRRoot),
      mMMRTotal(zOther.mMMRTotal),
      mCustomHash(zOther.mCustomHash),
      mTxBodyHash(zOther.mTxBodyHash)
{
    // Manually deep-copy the unique_ptr for mMagic
    // This assumes Magic has a copy constructor
    if (zOther.mMagic) {
        mMagic = std::make_unique<Magic>(*zOther.mMagic);
    } else {
        mMagic = nullptr;
    }
}

MiniData TxHeader::getBodyHash() const {
    return mTxBodyHash;
}

JSONObject TxHeader::toJSON() const {
    JSONObject txpow;

    txpow.put("chainid", mChainID.toString());
    txpow.put("block", mBlockNumber.toString());
    txpow.put("blkdiff", mBlockDifficulty.to0xString());

    // Super parents RLE encoded
    txpow.put("cascadelevels", GlobalParams::MINIMA_CASCADE_LEVELS);

    JSONArray supers;
    bool have_old = false;
    MiniData old;
    int counter = 0;

    const int levels = GlobalParams::MINIMA_CASCADE_LEVELS;
    for (int i = 0; i < levels; ++i) {
        const MiniData& curr = mSuperParents[static_cast<std::size_t>(i)];

        if (!have_old) {
            old = curr;
            counter = 1;
            have_old = true;
        } else {
            if (old.isEqual(curr)) {
                ++counter;
            } else {
                // Write the old run
                JSONObject sp;
                sp.put("difficulty", i - 1);
                sp.put("count", counter);
                sp.put("parent", old.to0xString());
                supers.add(sp);

                // Reset
                old = curr;
                counter = 1;
            }
        }

        // If last one, write current run
        if (i == levels - 1 && have_old) {
            JSONObject sp;
            sp.put("difficulty", i);
            sp.put("count", counter);
            sp.put("parent", curr.to0xString());
            supers.add(sp);
        }
    }
    txpow.put("superparents", supers);

    // Magic JSON
    if (mMagic) {
        txpow.put("magic", mMagic->toJSON());
    }

    txpow.put("mmr", mMMRRoot.toString());
    txpow.put("total", mMMRTotal.toString());

    txpow.put("customhash", mCustomHash.to0xString());
    txpow.put("txbodyhash", mTxBodyHash.to0xString());
    txpow.put("nonce", mNonce.toString());
    txpow.put("timemilli", mTimeMilli.toString());
    txpow.put("date", format_date_millis(mTimeMilli.getAsLong()));

    return txpow;
}

void TxHeader::writeDataStream(std::ostream& out) {
    // Basic header fields
    mNonce.writeDataStream(out);
    mChainID.writeDataStream(out);
    mTimeMilli.writeDataStream(out);
    mBlockNumber.writeDataStream(out);
    mBlockDifficulty.writeDataStream(out);

    // Super parents RLE
    bool have_parent = false;
    MiniData sparent;
    int counter = 0;
    const int levels = GlobalParams::MINIMA_CASCADE_LEVELS;

    for (int i = 0; i < levels; ++i) {
        const MiniData& curr = mSuperParents[static_cast<std::size_t>(i)];
        if (!have_parent) {
            sparent = curr;
            counter = 1;
            have_parent = true;
        } else {
            if (sparent.isEqual(curr)) {
                ++counter;
            } else {
                // Write the previous run
                MiniByte count(counter);
                count.writeDataStream(out);
                sparent.writeHashToStream(out);

                // Reset
                sparent = curr;
                counter = 1;
            }
        }

        // Last one: write current run
        if (i == levels - 1 && have_parent) {
            MiniByte count(counter);
            count.writeDataStream(out);
            sparent.writeHashToStream(out);
        }
    }

    // MMR state
    mMMRRoot.writeHashToStream(out);
    mMMRTotal.writeDataStream(out);

    // Magic
    // Always write magic stream (Java always has an instance)
    mMagic->writeDataStream(out);

    // Custom and body hashes
    mCustomHash.writeHashToStream(out);
    mTxBodyHash.writeHashToStream(out);
}

void TxHeader::readDataStream(std::istream& in) {
    // Basic fields
    mNonce           = MiniNumber::ReadFromStream(in);
    mChainID         = MiniData::ReadFromStream(in);
    mTimeMilli       = MiniNumber::ReadFromStream(in);
    mBlockNumber     = MiniNumber::ReadFromStream(in);
    mBlockDifficulty = MiniData::ReadFromStream(in);

    // Super parents RLE decode
    int tot = 0;
    const int levels = GlobalParams::MINIMA_CASCADE_LEVELS;
    if (levels < 0) {
        throw std::ios_base::failure("Invalid cascade levels");
    }
    mSuperParents.clear();
    mSuperParents.resize(static_cast<std::size_t>(levels), MiniData());

    while (tot < levels) {
        MiniByte len = MiniByte::ReadFromStream(in);
        MiniData sup = MiniData::ReadHashFromStream(in);
        int count = len.getValue();
        for (int i = 0; i < count; ++i) {
            if (tot >= levels) {
                throw std::ios_base::failure("SuperParents overflow during RLE decode");
            }
            mSuperParents[static_cast<std::size_t>(tot++)] = sup;
        }
    }

    // MMR state
    mMMRRoot  = MiniData::ReadHashFromStream(in);
    mMMRTotal = MiniNumber::ReadFromStream(in);

    // Magic
    if (!mMagic) {
        mMagic = std::make_unique<org::minima::objects::Magic>();
    }
    *mMagic = org::minima::objects::Magic::ReadFromStream(in);

    // Custom and body hashes
    mCustomHash = MiniData::ReadHashFromStream(in);
    mTxBodyHash = MiniData::ReadHashFromStream(in);
}

TxHeader TxHeader::ReadFromStream(std::istream& in) {
    TxHeader txp;
    txp.readDataStream(in);
    return txp;
}

TxHeader TxHeader::convertMiniDataVersion(const MiniData& zTxpData) {
    TxHeader txpow; // default
    try {
        const std::vector<std::uint8_t>& bytes = zTxpData.getBytes();
        std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::istringstream dis(buf, std::ios::binary);
        txpow = TxHeader::ReadFromStream(dis);
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
    return txpow;
}

} // namespace objects
} // namespace minima
} // namespace org