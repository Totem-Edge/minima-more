#include "org/minima/objects/tx_block.hpp"

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <vector> // Include vector for std::vector usage

#include "org/minima/utils/minima_logger.hpp"

// Full headers for all used types
#include "org/minima/objects/tx_po_w.hpp" // <--- ADDED THIS INCLUDE
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp" // For MMR::updateEntry


namespace org {
namespace minima {
namespace objects {

using org::minima::utils::MinimaLogger;

// Destructor and special members
TxBlock::~TxBlock() = default;
TxBlock::TxBlock(TxBlock&&) noexcept = default;
TxBlock& TxBlock::operator=(TxBlock&&) noexcept = default;

// Private default constructor
TxBlock::TxBlock() = default;

// Test constructor
TxBlock::TxBlock(const org::minima::objects::TxPoW& zTxPoW)
     : mTxPoW(zTxPoW.deepCopy()) {}

// Copy Contructor
TxBlock::TxBlock(const TxBlock& zOther)
    : mPreviousPeaks(zOther.mPreviousPeaks),
      mSpentCoins(zOther.mSpentCoins),
      mNewCoins(zOther.mNewCoins),
      mRemovedStates(zOther.mRemovedStates)
{
    // Manually deep-copy the unique_ptr
    if (zOther.mTxPoW) {
        mTxPoW = zOther.mTxPoW->deepCopy();
    } else {
        mTxPoW = nullptr;
    }
}

// Main constructor
TxBlock::TxBlock(org::minima::objects::mmr::MMR& zParentMMR,
                  const org::minima::objects::TxPoW& zTxPoW,
                  const std::vector<org::minima::objects::TxPoW*>& zAllTrans)
 {
    // try {
    //     MinimaLogger::log("DEBUG TxBlock ctor: Input zTxPoW block=" + zTxPoW.getBlockNumber().toString());
    // } catch (...) {
    //     MinimaLogger::log("DEBUG TxBlock ctor: Input zTxPoW INVALID - can't get block number!");
    // }
    
    // Main Block
    mTxPoW = zTxPoW.deepCopy();
    
    // std::string status = mTxPoW ? "not-null" : "NULL";
    // try {
    //     if (mTxPoW) {
    //         MinimaLogger::log("DEBUG TxBlock ctor: After deepCopy - block=" + mTxPoW->getBlockNumber().toString());
    //     }
    // } catch (...) {
    //     MinimaLogger::log("DEBUG TxBlock ctor: After deepCopy - mTxPoW exists but INVALID (can't get block)");
    // }

    // Get the Previous Peaks
    mPreviousPeaks = zParentMMR.getPeaks();

    // Make a new child MMR that you can play with
    org::minima::objects::mmr::MMR copymmr(&zParentMMR);

    // Cycle through the Main Block TxPoW
    calculateCoins(copymmr, zTxPoW);

    // Now cycle through the txns in the block MUST BE THE CORRECT ORDER for MMR root
    // getBlockTransactions now requires full TxPoW definition
    std::vector<org::minima::objects::base::MiniData> txns = zTxPoW.getBlockTransactions();
    for (const auto& txnid : txns) {
        // Get the correct txpow - the order could be wrong in the function param
        org::minima::objects::TxPoW* txp = getTxpoWFromList(txnid, zAllTrans);
        if (txp == nullptr) {
            // getTxPoWIDData now requires full TxPoW definition
            std::string msg = std::string("SERIOUS ERROR : TxBlock creation with missing txns.. ")
                            + zTxPoW.getTxPoWIDData().to0xString()
                            + " missing " + txnid.to0xString();
            throw std::invalid_argument(msg);
        }

        // And now process that
        calculateCoins(copymmr, *txp);
    }
}


org::minima::objects::TxPoW* TxBlock::getTxpoWFromList(
    const org::minima::objects::base::MiniData& zTxPoWID,
    const std::vector<org::minima::objects::TxPoW*>& zAllTrans)
{
    for (auto* txp : zAllTrans) {
        // getTxPoWIDData now requires full TxPoW definition
        if (txp && txp->getTxPoWIDData().isEqual(zTxPoWID)) {
            return txp;
        }
    }
    return nullptr;
}

void TxBlock::calculateCoins(org::minima::objects::mmr::MMR& zPreviousMMR,
                             const org::minima::objects::TxPoW& zTxPoW)
{
    // getTransaction, getWitness etc require full TxPoW definition
    // First check the main transaction
    calculateCoins(zPreviousMMR, zTxPoW.getTransaction(), zTxPoW.getWitness());

    // And now the Burn Transaction
    calculateCoins(zPreviousMMR, zTxPoW.getBurnTransaction(), zTxPoW.getBurnWitness());
}

void TxBlock::calculateCoins(org::minima::objects::mmr::MMR& zPreviousMMR,
                             const org::minima::objects::Transaction& zTransaction,
                             const org::minima::objects::Witness& zWitness)
{

    // Get all the input coins
    const auto& coinspent = zWitness.getAllCoinProofs(); // Returns const vector<unique_ptr<CoinProof>>&

    // And now get all the proofs pointing to the previous block
    for (const auto& csp_up : coinspent) { // Iterate over unique_ptr references
        const org::minima::objects::CoinProof& csp = *csp_up; // Dereference unique_ptr

        // Get the Coin shared_ptr
        auto null_deleter = [](org::minima::objects::Coin*){};
        std::shared_ptr<org::minima::objects::Coin> coin_sp(
            &const_cast<org::minima::objects::Coin&>(csp.getCoin()),
            null_deleter
        );
        if (!coin_sp) {
             throw std::runtime_error("CoinProof contains null Coin shared_ptr");
        }
        const org::minima::objects::Coin& coin = *coin_sp; // Dereference shared_ptr

        // Get the ENTRY Number
        org::minima::objects::mmr::MMREntryNumber entry = coin.getMMREntryNumber();

        // Add this to the MMR - so we can get a proof..
        zPreviousMMR.updateEntry(entry, csp.getMMRProof(), *csp.getMMRData());

        // The Proof - from the previous block
        org::minima::objects::mmr::MMRProof proof = zPreviousMMR.getProofToPeak(entry);

        // Construct the CoinProof
        auto proof_sp = std::make_shared<org::minima::objects::mmr::MMRProof>(proof);
        org::minima::objects::CoinProof cp(coin_sp, proof_sp);

        // Add to the list
        mSpentCoins.emplace_back(std::move(cp));
    }

    // The state of this Txn
    const auto& txnstate = zTransaction.getCompleteState(); // Returns const vector<unique_ptr<StateVariable>>&

    // All the Outputs
    const auto& outputs = zTransaction.getAllOutputs(); // Returns const vector<unique_ptr<Coin>>&
    
    // This logic MUST run even if coinspent is empty (for coinbase).
    
    org::minima::objects::base::MiniData basecoinid;
    if (coinspent.empty()) {
        // This is a coinbase transaction (no inputs).
        // This is NOT Genesis (Genesis has one input).
        basecoinid = zTransaction.getTransactionID();

    } else {
        // This is a normal transaction OR Genesis (has inputs)
        
        // CHECK FOR GENESIS (Block time == 1)
        if (zPreviousMMR.getBlockTime().isEqual(org::minima::objects::base::MiniNumber::ONE())) {
            // Genesis logic 
            basecoinid = zTransaction.getTransactionID();
        } else {
            // Normal logic 
            basecoinid = coinspent[0]->getCoin().getCoinID();
        }
    }

    // All the new coins (This loop was incorrectly skipped before)
    int num = 0;
    for (const auto& newoutput_up : outputs) { // Iterate over unique_ptr references
        const org::minima::objects::Coin& newoutput = *newoutput_up; // Dereference unique_ptr

        // Calculate the Correct CoinID for this coin
        org::minima::objects::base::MiniData coinid = const_cast<org::minima::objects::Transaction&>(zTransaction).calculateCoinID(basecoinid, num);

        // Create a new coin with correct coinid
        org::minima::objects::Coin correctcoin = std::move(*newoutput.getSameCoinWithCoinID(coinid));

        // Set the correct state variables
        if (correctcoin.storeState()) {
            std::vector<std::unique_ptr<StateVariable>> state_ptrs;
            state_ptrs.reserve(txnstate.size());
            for(const auto& sv_ptr : txnstate) {
                state_ptrs.push_back(std::make_unique<StateVariable>(*sv_ptr));
            }
            correctcoin.setState(std::move(state_ptrs));
        } else {
            std::vector<StateVariable> statecopy_obj;
            statecopy_obj.reserve(txnstate.size());
            for(const auto& sv_ptr : txnstate) {
                 statecopy_obj.push_back(*sv_ptr);
            }
            mRemovedStates[coinid.to0xString()] = std::move(statecopy_obj);
        }

        // Is this a create token output
        if (newoutput.getTokenID().isEqual(org::minima::objects::Token::TOKENID_CREATE)) {
            // Get the Create token details
            const org::minima::objects::Token* creator_ptr = newoutput.getToken();
            if (!creator_ptr) {
                 throw std::runtime_error("Token creation coin has null token pointer");
            }

            // Get the details
             // getBlockNumber now requires full TxPoW definition
            org::minima::objects::Token newtoken(
                coinid,
                creator_ptr->getScale(),
                newoutput.getAmount(),
                creator_ptr->getName(),
                creator_ptr->getTokenScript(),
                mTxPoW->getBlockNumber()
            );

            // Set it
            correctcoin.resetTokenID(*newtoken.getTokenID());

            // And set that as the token
            correctcoin.setToken(std::make_unique<org::minima::objects::Token>(std::move(newtoken)));
        }

        // Add to our list
        mNewCoins.emplace_back(std::move(correctcoin));

        // Next coin down
        num++;
    }
    
}

const org::minima::objects::TxPoW& TxBlock::getTxPoW() const {
    // MinimaLogger::log("DEBUG TxBlock::getTxPoW called");
    
    if (!mTxPoW) {
        // MinimaLogger::log("DEBUG TxBlock::getTxPoW - mTxPoW is NULL!");
        throw std::runtime_error("TxBlock::getTxPoW called but mTxPoW is null");
    }
    
    // MinimaLogger::log("DEBUG TxBlock::getTxPoW - mTxPoW is valid, returning reference");
    return *mTxPoW;
}

const std::vector<org::minima::objects::mmr::MMREntry>& TxBlock::getPreviousPeaks() const {
    return mPreviousPeaks;
}

const std::vector<org::minima::objects::CoinProof>&
TxBlock::getInputCoinProofs() const {
    return mSpentCoins;
}

const std::vector<org::minima::objects::Coin>&
TxBlock::getOutputCoins() const {
    return mNewCoins;
}

const std::vector<org::minima::objects::StateVariable>*
TxBlock::removedState(const std::string& zCoinID) const {
    auto it = mRemovedStates.find(zCoinID);
    if (it != mRemovedStates.end()) {
        return &it->second;
    }
    return nullptr;
}

void TxBlock::writeDataStream(std::ostream& out) {
    // mTxPoW
    if (!mTxPoW) {
        throw std::runtime_error("TxBlock::writeDataStream - mTxPoW is null");
    }
    // writeDataStream now requires full TxPoW definition
    mTxPoW->writeDataStream(out);

    // mPreviousPeaks
    org::minima::objects::base::MiniNumber::WriteToStream(out, static_cast<int>(mPreviousPeaks.size()));
    for (const auto& entry : mPreviousPeaks) {
        const_cast<org::minima::objects::mmr::MMREntry&>(entry).writeDataStream(out);
    }

    // mSpentCoins
    org::minima::objects::base::MiniNumber::WriteToStream(out, static_cast<int>(mSpentCoins.size()));
    for (const auto& cp : mSpentCoins) {
        const_cast<org::minima::objects::CoinProof&>(cp).writeDataStream(out);
    }

    // mNewCoins
    org::minima::objects::base::MiniNumber::WriteToStream(out, static_cast<int>(mNewCoins.size()));
    for (const auto& cc : mNewCoins) {
        const_cast<org::minima::objects::Coin&>(cc).writeDataStream(out);
    }
}

void TxBlock::readDataStream(std::istream& in) {
    mPreviousPeaks.clear();
    mSpentCoins.clear();
    mNewCoins.clear();
    mRemovedStates.clear();

    // mTxPoW
    mTxPoW = org::minima::objects::TxPoW::ReadFromStream(in);

    // mPreviousPeaks
    {
        int len = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
        mPreviousPeaks.reserve(len);
        for (int i = 0; i < len; ++i) {
            org::minima::objects::mmr::MMREntry entry = org::minima::objects::mmr::MMREntry::ReadFromStream(in);
            mPreviousPeaks.emplace_back(std::move(entry));
        }
    }

    // mSpentCoins
    {
        int len = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
        mSpentCoins.reserve(len);
        for (int i = 0; i < len; ++i) {
            org::minima::objects::CoinProof cp = std::move(*org::minima::objects::CoinProof::ReadFromStream(in));
            mSpentCoins.emplace_back(std::move(cp));
        }
    }

    // mNewCoins
    {
        int len = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
        mNewCoins.reserve(len);
        for (int i = 0; i < len; ++i) {
            auto cc = org::minima::objects::Coin::ReadFromStream(in);
            mNewCoins.emplace_back(std::move(*cc));
        }
    }
}

std::unique_ptr<TxBlock> TxBlock::ReadFromStream(std::istream& in) {
    auto sb = std::unique_ptr<TxBlock>(new TxBlock());
    sb->readDataStream(in);
    return sb;
}

std::unique_ptr<TxBlock> TxBlock::convertMiniDataVersion(const org::minima::objects::base::MiniData& zTxpData) {
    const std::vector<std::uint8_t>& bytes = zTxpData.getBytes();
    std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream iss(buf, std::ios::binary);

    std::unique_ptr<TxBlock> sync;
    try {
        // Convert data into a TxBlock
        sync = TxBlock::ReadFromStream(iss);
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        sync = nullptr;
    }
    return sync;
}

} // namespace objects
} // namespace minima
} // namespace org
