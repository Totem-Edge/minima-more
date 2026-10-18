#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org { namespace minima { namespace objects {
class TxHeader;
class TxBody;
class Transaction;
class Witness;
class Magic;
} } }

namespace org {
namespace minima {
namespace objects {

class TxPoW : public org::minima::utils::Streamable {
public:
    // Constructors
    TxPoW();
    TxPoW(const std::string& zTxPoWID, int zBlock, int zWeight);
    TxPoW(const std::string& zTxPoWID, int zBlock, int zWeight, bool zIsBlock, const std::string& zParent, bool zIsTransaction);

    // Destructor and move operations (PIMPL/unique_ptr to incomplete types)
    virtual ~TxPoW();
    TxPoW(TxPoW&&) noexcept;
    TxPoW& operator=(TxPoW&&) noexcept;

    // Delete copy
    TxPoW(const TxPoW&) = delete;
    TxPoW& operator=(const TxPoW&) = delete;

    // TEST function
    void addTestTransaction(const std::string& zTxPoWID);

    // Access/utility
    std::vector<std::string> getTransactions() const;

    // Header/Body getters
    TxHeader& getTxHeader();
    const TxHeader& getTxHeader() const;

    org::minima::objects::base::MiniData getTxHeaderBodyHash() const;
    void setHeaderBodyHash(); // throws if no body

    bool isMonotonic() const; // throws if no body

    TxBody* getTxBody();
    const TxBody* getTxBody() const;

    bool hasBody() const;
    void clearBody();

    // Header field accessors/setters
    void setNonce(const org::minima::objects::base::MiniNumber& zNonce);
    org::minima::objects::base::MiniNumber getNonce() const;

    org::minima::objects::base::MiniData getChainID() const;

    const Magic& getMagic() const;
    void setMagic(const Magic& zMagic);

    // Body field accessors/setters
    void setTxDifficulty(const org::minima::objects::base::MiniData& zDifficulty);
    org::minima::objects::base::MiniData getTxnDifficulty() const;

    Transaction& getTransaction();             // throws if no body
    const Transaction& getTransaction() const; // throws if no body

    Transaction& getBurnTransaction();             // throws if no body
    const Transaction& getBurnTransaction() const; // throws if no body

    void setTransaction(const Transaction& zTran); // throws if no body (deep copy)
    void setWitness(const Witness& zWitness);      // throws if no body (deep copy)
    void setBurnTransaction(const Transaction& zTran); // throws if no body (deep copy)
    void setBurnWitness(const Witness& zWitness);      // throws if no body (deep copy)

    Witness& getWitness();             // throws if no body
    const Witness& getWitness() const; // throws if no body

    Witness& getBurnWitness();             // throws if no body
    const Witness& getBurnWitness() const; // throws if no body

    void addBlockTxPOW(const org::minima::objects::base::MiniData& zTxPOWID); // throws if no body
    std::vector<org::minima::objects::base::MiniData> getBlockTransactions() const;

    org::minima::objects::base::MiniData getBlockDifficulty() const;
    void setBlockDifficulty(const org::minima::objects::base::MiniData& zBlockDifficulty);

    org::minima::objects::base::MiniData getParentID() const;
    void setSuperParent(int zLevel, const org::minima::objects::base::MiniData& zSuperParent);
    org::minima::objects::base::MiniData getSuperParent(int zLevel) const;

    org::minima::objects::base::MiniNumber getBurn() const; // sums non-const Transaction methods safely

    void setTimeMilli();
    void setTimeMilli(const org::minima::objects::base::MiniNumber& zMilli);
    org::minima::objects::base::MiniNumber getTimeMilli() const;

    void setBlockNumber(const org::minima::objects::base::MiniNumber& zBlockNum);
    org::minima::objects::base::MiniNumber getBlockNumber() const;

    org::minima::objects::base::MiniData getMMRRoot() const;
    void setMMRRoot(const org::minima::objects::base::MiniData& zRoot);

    org::minima::objects::base::MiniNumber getMMRTotal() const;
    void setMMRTotal(const org::minima::objects::base::MiniNumber& zTotal);

    org::minima::objects::base::MiniData getCustomHash() const;
    void setCustomHash(const org::minima::objects::base::MiniData& zCustomHash);

    int getCheckRejectNumber() const;
    void incrementCheckRejectNumber();

    // JSON
    org::minima::utils::json::JSONObject toJSON() const;
    std::string toString() const;

    // Streamable
    // FIX: Removed 'const' to match the Streamable base class
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    static std::unique_ptr<TxPoW> ReadFromStream(std::istream& in);

    // Deep copy via serialization
    std::unique_ptr<TxPoW> deepCopy() const;

    // Convert a MiniData version into a TxPoW
    static std::unique_ptr<TxPoW> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);

    // Compute the transaction hash for both
    void calculateTransactionID(); // throws if no body

    // Internal getters
    std::string getTxPoWID() const;
    org::minima::objects::base::MiniData getTxPoWIDData() const;

    int getSuperLevel() const;
    bool isBlock() const;
    // BigDecimal -> string per mapping
    std::string getWeight() const;

    bool isTransaction() const;
    std::int64_t getSizeinBytes() const;
    std::int64_t getSizeinBytesWithoutBlockTxns() const;

    // Calculate all derived fields - must be called post read
    void calculateTXPOWID();

private:
    // Helper
    static int getSuperLevel(const org::minima::objects::base::MiniData& zBlockDifficulty,
                             const org::minima::objects::base::MiniData& zTxPoWID);

    // Member-wise deep copy (no serialization round-trip, no const_cast)
    void copyFrom(const TxPoW& zOther);

private:
    // Owned components
    std::unique_ptr<TxHeader> mHeader;
    std::unique_ptr<TxBody>   mBody;

    // Internal state
    std::string mTxPOWIDStr = "0x00";
    org::minima::objects::base::MiniData mTxPOWID = org::minima::objects::base::MiniData::ZERO_TXPOWID();
    int  mSuperBlock = 0;
    std::int64_t mTxPoWSize = 0;
    // BigDecimal mapped to string for precision
    std::string mBlockWeight = "0";

    // Checking
    int mCheckNumber = 0;

    // Test params
    bool mIsTesting = false;
    org::minima::objects::base::MiniNumber mTestBlockNumber = org::minima::objects::base::MiniNumber::ZERO();
    bool mTestIsBlock = true;
    bool mTestIsTxn = false;
    org::minima::objects::base::MiniData mTestParent = org::minima::objects::base::MiniData::ZERO_TXPOWID();
    std::vector<std::string> mTestTransactions;

protected:
    // Accessible to subclasses (e.g. GenesisTxPoW) — matches Java's protected access
    bool mIsBlockPOW = false;
    bool mIsTxnPOW = false;
};

} // namespace objects
} // namespace minima
} // namespace org
