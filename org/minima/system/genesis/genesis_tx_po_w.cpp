#include "org/minima/system/genesis/genesis_tx_po_w.hpp"

// Full headers for all used types (per SPECIAL INSTRUCTIONS)
#include <chrono>
#include <memory>
#include <vector>

#include "org/minima/system/genesis/genesis_m_m_r.hpp"
#include "org/minima/system/genesis/genesis_coin.hpp"

#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/witness.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

#include "org/minima/system/params/global_params.hpp"

#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace system {
namespace genesis {

using org::minima::objects::Transaction;
using org::minima::objects::Witness;
using org::minima::objects::Coin;
using org::minima::objects::ScriptProof;
using org::minima::objects::Token;
using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::Address;

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

using org::minima::objects::mmr::MMRData;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::objects::mmr::MMRProof;

using org::minima::database::txpowtree::TxPoWTreeNode;

GenesisTxPoW::GenesisTxPoW(const std::string& zGenesisAddress)
    : TxPoW() {

    // The first BASE MMR..
    GenesisMMR genesismmr;

    // Set difficulties
    setTxDifficulty(org::minima::utils::Crypto::MAX_HASH());

    // Nonce
    setNonce(MiniNumber(256));

    // Current time in millis
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    setTimeMilli(MiniNumber(static_cast<long long>(now_ms)));

    // First Block starts at 1! .. 0 created the genesis coin
    setBlockNumber(MiniNumber::ONE());

    setBlockDifficulty(org::minima::utils::Crypto::MAX_HASH());

    // Super Block Levels.. FIRST just copy them all..
    MiniData ultimateparent("0x00");
    for (int i = 0; i < org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS; ++i) {
        setSuperParent(i, ultimateparent);
    }

    // Set the Genesis transaction
    Transaction& transaction = getTransaction();

    // The first billion Minima - add the GenesisCoin as input
    transaction.addInput(std::make_unique<org::minima::system::genesis::GenesisCoin>());

    // Now add 1 output to the provided address
    {
        auto outcoin = std::make_unique<Coin>(
            Coin::COINID_OUTPUT,
            MiniData(zGenesisAddress),
            MiniNumber::BILLION(),
            Token::TOKENID_MINIMA
        );
        transaction.addOutput(std::move(outcoin));
    }

    // Add a coinproof..
    Witness& witness = getWitness();

    // Get the proof..
    MMRProof proof = genesismmr.getProofToPeak(MMREntryNumber::ZERO);

    // Create the CoinProof and add it to the witness..
    {
        auto cptr   = std::make_shared<org::minima::system::genesis::GenesisCoin>();
        auto pptr   = std::make_shared<MMRProof>(proof);
        auto cproof = std::make_unique<org::minima::objects::CoinProof>(cptr, pptr);
        witness.addCoinProof(std::move(cproof));
    }

    // And the script is Return True..
    {
        std::string truescript = Address::getTrueAddress().getScript();
        auto sproof = std::make_unique<ScriptProof>(truescript);
        witness.addScript(std::move(sproof));
    }

    // Set the body hash - no more changes..
    setHeaderBodyHash();

    // Calculate the TxPOWID (transaction hash) for transactions
    calculateTransactionID();

    // Create a TxBlock.. (TxBlock takes TxPoW by value; move a deep copy)
    // The vector is intentionally empty — genesis block has no child transactions.
    // (Matches Java: new ArrayList<>())
    std::vector<TxPoW*> empty;
    auto txpowcopy = this->deepCopy(); // unique_ptr<TxPoW>
    auto txblock = std::make_shared<TxBlock>(genesismmr, std::move(*txpowcopy), empty);

    // And the MMR details
    TxPoWTreeNode node(*txblock, false);

    // Get the MMR root data (MMR::getRoot returns unique_ptr<MMRData>)
    auto root = node.getMMR().getRoot();
    setMMRRoot(root->getData());
    setMMRTotal(root->getValue());

    // Set the TXPOW
    calculateTXPOWID();

    // Get the TxPoWID - this is a one time universal value
    std::string gentxpow = getTxPoWID();
    org::minima::utils::MinimaLogger::log("Genesis block created : " + gentxpow);

    // Hard code it.. (matches Java GenesisTxPoW.java:102-103)
    mIsBlockPOW = true;
    mIsTxnPOW   = true;
}

GenesisTxPoW::~GenesisTxPoW() = default;
GenesisTxPoW::GenesisTxPoW(GenesisTxPoW&&) noexcept = default;
GenesisTxPoW& GenesisTxPoW::operator=(GenesisTxPoW&&) noexcept = default;

} // namespace genesis
} // namespace system
} // namespace minima
} // namespace org