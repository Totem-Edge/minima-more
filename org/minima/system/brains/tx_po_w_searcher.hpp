#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>

namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; class TxPowTree; } } } }
namespace org { namespace minima { namespace database { namespace wallet { class Wallet; } } } }
namespace org { namespace minima { namespace database { namespace txpowdb { class TxPoWDB; } } } }
namespace org { namespace minima { namespace objects { class Coin; class Token; class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace objects { namespace mmr { class MegaMMR; } } } }

namespace org {
namespace minima {
namespace system {
namespace brains {

class TxPoWSearcher {
public:
    // Coins (unspent / relevant / floating)
    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    getAllRelevantUnspentCoins(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartNode);

    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    getRelevantUnspentCoins(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartNode,
                            const std::string& zTokenID, bool zSimpleOnly);

    static std::shared_ptr<org::minima::objects::Coin>
    searchCoin(const org::minima::objects::base::MiniData& zCoinID);

    static std::shared_ptr<org::minima::objects::Coin>
    searchCoin(const org::minima::objects::base::MiniData& zCoinID, bool zMegaMMR);

    static std::shared_ptr<org::minima::objects::Coin>
    getFloatingCoin(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartNode,
                    const org::minima::objects::base::MiniNumber& zAmount,
                    const org::minima::objects::base::MiniData& zAddress,
                    const org::minima::objects::base::MiniData& zTokenID);

    // Overloaded searchCoins
    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    searchCoins(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartNode, bool zRelevant,
                bool zCheckCoinID, const org::minima::objects::base::MiniData& zCoinID,
                bool zCheckAmount, const org::minima::objects::base::MiniNumber& zAmount,
                bool zCheckAddress, const org::minima::objects::base::MiniData& zAddress,
                bool zCheckTokenID, const org::minima::objects::base::MiniData& zTokenID,
                bool zSimpleOnly);

    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    searchCoins(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartNode, bool zRelevant,
                bool zCheckCoinID, const org::minima::objects::base::MiniData& zCoinID,
                bool zCheckAmount, const org::minima::objects::base::MiniNumber& zAmount,
                bool zCheckAddress, const org::minima::objects::base::MiniData& zAddress,
                bool zCheckTokenID, const org::minima::objects::base::MiniData& zTokenID,
                bool zSimpleOnly, int zDepth);

    static std::vector<std::shared_ptr<org::minima::objects::Coin>>
    searchCoins(const std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>& zStartNode, bool zRelevant,
                bool zCheckCoinID, const org::minima::objects::base::MiniData& zCoinID,
                bool zCheckAmount, const org::minima::objects::base::MiniNumber& zAmount,
                bool zCheckAddress, const org::minima::objects::base::MiniData& zAddress,
                bool zCheckTokenID, const org::minima::objects::base::MiniData& zTokenID,
                bool zCheckState, const std::string& zState, bool zWildCardState,
                bool zSimpleOnly, int zDepth, bool zMEGAMMR);

    // Tree and chain searches
    static std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>
    getTreeNodeForCoin(const org::minima::objects::base::MiniData& zCoinID);

    static std::shared_ptr<org::minima::objects::TxPoW>
    getTxPoWBlock(const org::minima::objects::base::MiniNumber& zBlockNumber);

    static std::shared_ptr<org::minima::objects::TxPoW>
    searchChainForTxPoW(const org::minima::objects::base::MiniData& zTxPoWID);

    static std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode>
    searchChainForTxPoWBlock(const org::minima::objects::base::MiniData& zTxPoWID);

    // Address-based searches and relevance
    static std::vector<std::shared_ptr<org::minima::objects::TxPoW>>
    searchTxPoWviaAddress(const org::minima::objects::base::MiniData& zAddress);

    static bool
    checkTxPoWForAddress(const org::minima::objects::TxPoW& zTxPoW,
                         const org::minima::objects::base::MiniData& zAddress);

    static bool
    checkTxPoWRelevant(const org::minima::objects::TxPoW& zTxPoW,
                       org::minima::database::wallet::Wallet& zWallet);

    // Tokens (chain + imported)
    static std::vector<std::shared_ptr<org::minima::objects::Token>> getAllTokens();

    static void importToken(const std::shared_ptr<org::minima::objects::Token>& zToken);

    static std::shared_ptr<org::minima::objects::Token>
    getToken(const org::minima::objects::base::MiniData& zTokenID);

private:
    static std::mutex s_importedTokensMutex;
    static std::vector<std::shared_ptr<org::minima::objects::Token>> s_importedTokens;
};

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org