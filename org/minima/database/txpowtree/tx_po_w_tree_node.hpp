#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// Base class include (implements Streamable)
#include "org/minima/utils/streamable.hpp"

// Full include for MiniNumber (used as value member mTotalWeight)
#include "org/minima/objects/base/mini_number.hpp"

// Forward declarations for types used in members/signatures
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }
namespace org { namespace minima { namespace database { namespace txpowdb { class TxPoWDB; } } } }
namespace org { namespace minima { namespace database { namespace wallet { class Wallet; } } } }
namespace org { namespace minima { namespace objects { class Coin; class CoinProof; class StateVariable; class TxBlock; class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace objects { namespace mmr { class MMR; class MMRData; class MMREntry; class MMREntryNumber; class MMRProof; } } } }

namespace org { namespace minima { namespace database { namespace txpowtree {

// <-- FIXED: Must forward-declare types used in function signatures
// (These were used in the .cpp but not all were forwarded in the .hpp)
using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::Coin;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::database::wallet::Wallet;

class TxPoWTreeNode : public org::minima::utils::Streamable,
                      public std::enable_shared_from_this<TxPoWTreeNode> { // <-- FIXED: Added enable_shared_from_this
public:
    TxPoWTreeNode();

    // <-- FIXED: Constructors updated to match .cpp definitions
    explicit TxPoWTreeNode(const TxBlock& zTxBlock);
    TxPoWTreeNode(const TxBlock& zTxBlock, bool zFindRelevant);
    explicit TxPoWTreeNode(std::unique_ptr<TxBlock> zTxBlock);
    TxPoWTreeNode(std::unique_ptr<TxBlock> zTxBlock, bool zFindRelevant);
    explicit TxPoWTreeNode(const TxPoW& zTestTxPoW);


    // PIMPL-FIX for unique_ptr members
    virtual ~TxPoWTreeNode();
    TxPoWTreeNode(TxPoWTreeNode&&) noexcept;
    TxPoWTreeNode& operator=(TxPoWTreeNode&&) noexcept;
    TxPoWTreeNode(const TxPoWTreeNode&) = delete;
    TxPoWTreeNode& operator=(const TxPoWTreeNode&) = delete;

    // Accessors
    // <-- FIXED: All accessor return types updated to match .cpp
    // <-- FIXED: Added const overloads that were in .cpp but missing from .hpp
    TxBlock& getTxBlock();
    const TxBlock& getTxBlock() const;

    TxPoW& getTxPoW();
    const TxPoW& getTxPoW() const;

    MiniNumber getBlockNumber() const;

    MMR& getMMR();
    const MMR& getMMR() const;

    const std::vector<Coin>& getAllCoins() const;
    void addCoin(const Coin& zCoin);

    bool isRelevantEntry(const MMREntryNumber& zMMREntryNumber) const; // <-- FIXED: Pass by const-ref

    const std::vector<Coin>& getRelevantCoins() const; // <-- FIXED: Return type
    const std::vector<MMREntryNumber>& getRelevantCoinsEntries() const; // <-- FIXED: Return type
    void addRelevantCoin(const MMREntryNumber& zEntry);
    void removeRelevantCoin(const MMREntryNumber& zEntry); // <-- FIXED: Pass by const-ref

    void calculateRelevantCoins();

    void addChildNode(const std::shared_ptr<TxPoWTreeNode>& zTxPoWTreeNode);
    const std::vector<std::shared_ptr<TxPoWTreeNode>>& getChildren() const; // <-- FIXED: Return const-ref

    void setParent(const std::shared_ptr<TxPoWTreeNode>& zTxPoWTreeNode);
    std::shared_ptr<TxPoWTreeNode> getParent() const; // <-- FIXED: Add const
    std::shared_ptr<TxPoWTreeNode> getParent(int zBlocks) const; // <-- FIXED: Add const
    std::shared_ptr<TxPoWTreeNode> getPastNode(const MiniNumber& zBlockNumber) const; // <-- FIXED: Add const & param const-ref

    void copyParentRelevantCoins();
    void clearParent();

    void setTotalWeight(const MiniNumber& zWeight);
    void addToTotalWeight(const MiniNumber& zWeight);
    const MiniNumber& getTotalWeight() const;

    bool checkFullTxns(org::minima::database::txpowdb::TxPoWDB& zTxpDB);

    // Streamable
    virtual void writeDataStream(std::ostream& zOut) override;
    virtual void readDataStream(std::istream& zIn) override;

    static std::unique_ptr<TxPoWTreeNode> ReadFromStream(std::istream& zIn); // <-- FIXED: Return unique_ptr

    static void CheckTxBlockForNotifyCoins(const TxBlock& zBlock); // <-- FIXED: Pass by const-ref
    static void main(const std::vector<std::string>& zArgs);

private:
    // <-- FIXED: All these helper functions were missing from the header
    void constructMMR(bool zFindRelevant);
    bool checkRelevant(const Coin& zCoin, Wallet& zWallet) const;
    static std::unique_ptr<TxBlock> deepCopyTxBlock(const TxBlock& zTxBlock);


    // <-- FIXED: This is the main problem. It must be unique_ptr to match the .cpp
    std::unique_ptr<TxBlock> mTxBlock; 
    std::weak_ptr<TxPoWTreeNode> mParent;
    std::vector<std::shared_ptr<TxPoWTreeNode>> mChildren;
    MiniNumber mTotalWeight;
    std::unique_ptr<MMR> mMMR; // <-- FIXED: Changed to unique_ptr to match .cpp logic

    // <-- FIXED: Vector types changed to match .cpp
    std::vector<Coin> mCoins;
    std::vector<MMREntryNumber> mRelevantMMRCoins;
    std::vector<Coin> mComputedRelevantCoins;

    bool mHaveCheckedFull {false};
};

} } } } // namespace org::minima::database::txpowtree