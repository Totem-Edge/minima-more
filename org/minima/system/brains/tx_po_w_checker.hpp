#pragma once

#include <vector>
#include <string>

// Forward declarations
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; } } } }
namespace org { namespace minima { namespace objects { class TxPoW; class TxBlock; class Transaction; class Witness; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace objects { namespace mmr { class MMR; } } } }

namespace org {
namespace minima {
namespace system {
namespace brains {

class TxPoWChecker {
public:
    // Static network and timing parameters
    static org::minima::objects::base::MiniData CURRENT_NETWORK;
    static org::minima::objects::base::MiniNumber MAX_TIME_FUTURE;

    /**
     * Timed block check
     * FIX: zTxPoW must be NON-CONST because it gets modified by helpers
     */
    static bool checkTxPoWBlockTimed(org::minima::database::txpowtree::TxPoWTreeNode* zParentNode,
                                     org::minima::objects::TxPoW& zTxPoW, // NOT CONST
                                     std::vector<org::minima::objects::TxPoW*>& zTransactions);

    /**
     * Full block check
     * FIX: zTxPoW must be NON-CONST
     */
    static bool checkTxPoWBlock(org::minima::database::txpowtree::TxPoWTreeNode* zParentNode,
                                org::minima::objects::TxPoW& zTxPoW, // NOT CONST
                                std::vector<org::minima::objects::TxPoW*>& zTransactions);

    /**
     * checkTxBlockOnly - This function also calls helpers that modify, so zTxBlock must be NON-CONST
     */
    static bool checkTxBlockOnly(org::minima::database::txpowtree::TxPoWTreeNode* zParentNode,
                                 org::minima::objects::TxBlock& zTxBlock); // NOT CONST

    // Basic TxPoW transaction checks (main + burn)
    static bool checkTxPoWBasic(org::minima::objects::TxPoW& zTxPoW);

    // Simplified check (min work, size, relay policy, MMR, scripts)
    // FIX: Must be NON-CONST
    static bool checkTxPoWSimple(org::minima::objects::mmr::MMR& zTipMMR,
                                 org::minima::objects::TxPoW& zTxPoW, // NOT CONST
                                 org::minima::objects::TxPoW& zBlock, // NOT CONST
                                 bool zLog);

    // Scripts checks for a TxPoW (main + burn)
    // FIX: Must be NON-CONST
    static bool checkTxPoWScripts(org::minima::objects::mmr::MMR& zTipMMR,
                                  org::minima::objects::TxPoW& zTxPoW, // NOT CONST
                                  org::minima::objects::TxPoW& zBlock); // NOT CONST

    // MMR checks (main + burn)
    static bool checkMMR(org::minima::objects::mmr::MMR& zTipMMR,
                         org::minima::objects::TxPoW& zTxPoW);
    static bool checkMMR(org::minima::objects::mmr::MMR& zTipMMR,
                         org::minima::objects::TxPoW& zTxPoW, // NOT CONST
                         bool zLog);

    // Signature checks (main + burn)
    static bool checkSignatures(org::minima::objects::TxPoW& zTxPoW);

    // Mempool double-spend coin check
    static bool checkMemPoolCoins(org::minima::objects::TxPoW& zTxPoW);

    // Check that all Super Parents are correct
    // (This function only reads, so const is correct here)
    static bool checkParents(org::minima::database::txpowtree::TxPoWTreeNode& zTip,
                             const org::minima::objects::TxPoW& zBlock);

private:
    // Internal helpers
    static bool checkTxPoWBasic_impl(const std::string& zTxPoWID,
                                     org::minima::objects::Transaction& zTransaction,
                                     org::minima::objects::Witness& zWitness);

    // FIX: Must be NON-CONST
    static bool checkTxPoWScripts_impl(org::minima::objects::mmr::MMR& zTipMMR,
                                       org::minima::objects::Transaction& zTransaction, // NOT CONST
                                       org::minima::objects::Witness& zWitness,     // NOT CONST
                                       org::minima::objects::TxPoW& zBlock);     // NOT CONST

    // FIX: Must be NON-CONST
    static bool checkMMR_impl(org::minima::objects::mmr::MMR& zTipMMR,
                              org::minima::objects::Witness& zWitness, // NOT CONST
                              bool zLog);
};

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org