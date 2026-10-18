#pragma once

#include <memory>
#include <vector>
#include <string>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

// Namespaced forward declarations for project dependencies (Pitfall 10)
namespace org { namespace minima { namespace database { class MinimaDB; } } }

namespace org { namespace minima { namespace database { namespace txpowdb { class TxPoWDB; } } } }

namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; } } } }

namespace org { namespace minima { namespace database { namespace userprefs { class UserDB; } } } }

namespace org { namespace minima { namespace objects {
    class Coin;
    class CoinProof;
    class Magic;
    class Transaction;
    class TxBlock;
    class TxPoW;
    class Witness;
} } }

namespace org { namespace minima { namespace objects { namespace mmr {
    class MMR;
    class MMRData;
} } } }

namespace org { namespace minima { namespace system { namespace params { class GlobalParams; } } } }

namespace org { namespace minima { namespace utils {
    class Crypto;
    class MinimaLogger;
} } }

namespace org {
namespace minima {
namespace system {
namespace brains {

class TxPoWGenerator {
public:
    // Mempool status
    static bool isMempoolFull();
    static org::minima::objects::base::MiniNumber getMinMempoolBurn();

    // Difficulty data calculation: MAX_VAL / hashes
    static org::minima::objects::base::MiniData
    calculateDifficultyData(const org::minima::objects::base::MiniNumber& zHashes);

    // Generate a complete TxPoW
    static std::unique_ptr<org::minima::objects::TxPoW>
    generateTxPoW(const org::minima::objects::Transaction& zTransaction,
                  const org::minima::objects::Witness& zWitness);

    static std::unique_ptr<org::minima::objects::TxPoW>
    generateTxPoW(const org::minima::objects::Transaction& zTransaction,
                  const org::minima::objects::Witness& zWitness,
                  const org::minima::objects::Transaction* zBurnTransaction,
                  const org::minima::objects::Witness* zBurnWitness);

    // Next block difficulty (bounded retarget)
    static org::minima::objects::base::MiniData
    getBlockDifficulty(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zParent);

    // Chain speed (blocks per second)
    static org::minima::objects::base::MiniNumber
    getChainSpeed(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartBlock,
                  const org::minima::objects::base::MiniNumber& zBlocksBack);

    // Median time block
    static std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>
    getMedianTimeBlock(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartBlock);

    static std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>
    getMedianTimeBlock(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartBlock,
                       int zBlocksBack);

    // Precompute CoinIDs for a transaction's outputs based on first input
    static void precomputeTransactionCoinID(org::minima::objects::Transaction& zTransaction);

    // Test utility
    static void main(const std::vector<std::string>& zArgs);

private:
    // Static bounds for speed ratio
    static const org::minima::objects::base::MiniNumber MAX_SPBOUND_DIFFICULTY;
    static const org::minima::objects::base::MiniNumber MIN_SPBOUND_DIFFICULTY;

    // Mempool state
    static bool MEMPOOL_FULL;
    static org::minima::objects::base::MiniNumber MIN_MEMPOOL_BURN;
};

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org