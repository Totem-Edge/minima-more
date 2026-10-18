#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

// Forward declarations to avoid heavy includes in header (Rule 10)
namespace org { namespace minima { namespace objects { class Magic; } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace objects {

class TxHeader : public org::minima::utils::Streamable {
public:
    // Static network identifiers
    static org::minima::objects::base::MiniData MAIN_NET;
    static org::minima::objects::base::MiniData TEST_NET;

    // Constructors / destructor
    TxHeader();
    virtual ~TxHeader();                   // For unique_ptr<Magic> (PIMPL fix)
    TxHeader(TxHeader&&) noexcept;
    TxHeader& operator=(TxHeader&&) noexcept;

    // Delete copy semantics due to unique_ptr
    TxHeader(const TxHeader& zOther);
    TxHeader& operator=(const TxHeader&) = delete;

    // Accessors
    org::minima::objects::base::MiniData getBodyHash() const;

    // JSON representation (mirrors Java toJSON)
    org::minima::utils::json::JSONObject toJSON() const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static helpers (Java-style)
    static TxHeader ReadFromStream(std::istream& in);
    static TxHeader convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);

    // Public fields (kept to match Java structure)
    org::minima::objects::base::MiniNumber mNonce;            // default 0
    org::minima::objects::base::MiniData   mChainID;          // default MAIN_NET
    org::minima::objects::base::MiniNumber mTimeMilli;        // currentTimeMillis
    org::minima::objects::base::MiniNumber mBlockNumber;      // default 0
    org::minima::objects::base::MiniData   mBlockDifficulty;  // Crypto::MAX_HASH()

    // Super parents list (size = GlobalParams.MINIMA_CASCADE_LEVELS)
    std::vector<org::minima::objects::base::MiniData> mSuperParents;

    // Magic parameters
    std::unique_ptr<org::minima::objects::Magic> mMagic;

    // MMR state
    org::minima::objects::base::MiniData   mMMRRoot;   // default "0x00"
    org::minima::objects::base::MiniNumber mMMRTotal;  // MiniNumber::ZERO

    // Custom and body hashes
    org::minima::objects::base::MiniData mCustomHash;  // MiniData::ZERO_TXPOWID()
    org::minima::objects::base::MiniData mTxBodyHash;  // default "0x00"
};

} // namespace objects
} // namespace minima
} // namespace org