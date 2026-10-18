#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "org/minima/utils/streamable.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"

// Namespaced forward declarations (Pitfall 10)
namespace org { namespace minima { namespace objects {
class TxPoW;
class Coin;
class CoinProof;
class StateVariable;
class Transaction;
class Witness;
class Token;
} } }

namespace org { namespace minima { namespace objects { namespace mmr {
class MMR;
class MMRData;
class MMRProof;
class MMREntryNumber;
} } } }

namespace org {
namespace minima {
namespace objects {

class TxBlock : public org::minima::utils::Streamable {
public:
    // Destructor and special members (incomplete type fix for unique_ptr members)
    virtual ~TxBlock();
    TxBlock(TxBlock&&) noexcept;
    TxBlock& operator=(TxBlock&&) noexcept;

    // Delete copy operations
    TxBlock(const TxBlock& zOther);
    TxBlock& operator=(const TxBlock&) = delete;

    // For Tests
    explicit TxBlock(const org::minima::objects::TxPoW& zTxPoW); // Pass by value

    // Main constructor
    TxBlock(org::minima::objects::mmr::MMR& zParentMMR,
            const org::minima::objects::TxPoW& zTxPoW, // Pass by value
            const std::vector<org::minima::objects::TxPoW*>& zAllTrans);
    

    // Getters
    const org::minima::objects::TxPoW& getTxPoW() const;
    const std::vector<org::minima::objects::mmr::MMREntry>& getPreviousPeaks() const;
    const std::vector<org::minima::objects::CoinProof>& getInputCoinProofs() const;
    const std::vector<org::minima::objects::Coin>& getOutputCoins() const;

    // Removed state for a given CoinID (0x string). Returns nullptr if not present.
    const std::vector<org::minima::objects::StateVariable>*
    removedState(const std::string& zCoinID) const;

    // Streamable
    void writeDataStream(std::ostream& out) override;
    void readDataStream(std::istream& in) override;

    // Static factory: Read from stream
    static std::unique_ptr<TxBlock> ReadFromStream(std::istream& in);

    // Convert a MiniData version into a TxBlock
    static std::unique_ptr<TxBlock> convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData);

private:
    // Private default constructor
    TxBlock();

    // Helpers (Java private methods)
    org::minima::objects::TxPoW* getTxpoWFromList(const org::minima::objects::base::MiniData& zTxPoWID,
                                                   const std::vector<org::minima::objects::TxPoW*>& zAllTrans);

    void calculateCoins(org::minima::objects::mmr::MMR& zPreviousMMR, const org::minima::objects::TxPoW& zTxPoW);
    void calculateCoins(org::minima::objects::mmr::MMR& zPreviousMMR,
                        const org::minima::objects::Transaction& zTransaction,
                        const org::minima::objects::Witness& zWitness);

    // Members
    // The main TxPoW block
    std::unique_ptr<org::minima::objects::TxPoW> mTxPoW;

    // The MMR Peaks from the previous block
    std::vector<org::minima::objects::mmr::MMREntry> mPreviousPeaks;

    // The Proofs of all the input-spent coins - unspent as of the last block
    std::vector<org::minima::objects::CoinProof> mSpentCoins;

    // A list of all the newly created coins
    std::vector<org::minima::objects::Coin> mNewCoins;

    // Keep a record of removed states for notify events (not persisted)
    std::unordered_map<std::string, std::vector<org::minima::objects::StateVariable>> mRemovedStates;
};

} // namespace objects
} // namespace minima
} // namespace org