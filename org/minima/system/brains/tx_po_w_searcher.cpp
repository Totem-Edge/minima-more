#include "org/minima/system/brains/tx_po_w_searcher.hpp"

#include <unordered_set>
#include <algorithm>
#include <limits>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/transaction.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/system/params/general_params.hpp"

namespace org {
namespace minima {
namespace system {
namespace brains {

// Static members
std::mutex TxPoWSearcher::s_importedTokensMutex;
std::vector<std::shared_ptr<org::minima::objects::Token>> TxPoWSearcher::s_importedTokens;

using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowtree::TxPowTree;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::wallet::Wallet;
using org::minima::objects::Coin;
using org::minima::objects::StateVariable;
using org::minima::objects::Token;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

std::vector<std::shared_ptr<Coin>>
TxPoWSearcher::getAllRelevantUnspentCoins(const std::shared_ptr<TxPoWTreeNode>& zStartNode) {
    return searchCoins(zStartNode, true,
                       false, MiniData::ZERO_TXPOWID(),
                       false, MiniNumber::ZERO(),
                       false, MiniData::ZERO_TXPOWID(),
                       false, MiniData::ZERO_TXPOWID(),
                       false);
}

std::vector<std::shared_ptr<Coin>>
TxPoWSearcher::getRelevantUnspentCoins(const std::shared_ptr<TxPoWTreeNode>& zStartNode,
                                       const std::string& zTokenID, bool zSimpleOnly) {
    return searchCoins(zStartNode, true,
                       false, MiniData::ZERO_TXPOWID(),
                       false, MiniNumber::ZERO(),
                       false, MiniData::ZERO_TXPOWID(),
                       true, MiniData(zTokenID),
                       zSimpleOnly);
}

std::shared_ptr<Coin>
TxPoWSearcher::searchCoin(const MiniData& zCoinID) {
    return searchCoin(zCoinID, org::minima::system::params::GeneralParams::IS_MEGAMMR);
}

std::shared_ptr<Coin>
TxPoWSearcher::searchCoin(const MiniData& zCoinID, bool zMegaMMR) {
    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    auto coins = searchCoins(tip, false,
                             true, zCoinID,
                             false, MiniNumber::ZERO(),
                             false, MiniData::ZERO_TXPOWID(),
                             false, MiniData::ZERO_TXPOWID(),
                             false, std::string(), false,
                             false, std::numeric_limits<int>::max(), zMegaMMR);

    if (!coins.empty()) {
        return coins.front();
    }
    return nullptr;
}

std::shared_ptr<Coin>
TxPoWSearcher::getFloatingCoin(const std::shared_ptr<TxPoWTreeNode>& zStartNode,
                               const MiniNumber& zAmount,
                               const MiniData& zAddress,
                               const MiniData& zTokenID) {
    auto coins = searchCoins(zStartNode, false,
                             false, MiniData::ZERO_TXPOWID(),
                             true, zAmount,
                             true, zAddress,
                             true, zTokenID,
                             false);
    if (!coins.empty()) {
        return coins.front();
    }
    return nullptr;
}

std::vector<std::shared_ptr<Coin>>
TxPoWSearcher::searchCoins(const std::shared_ptr<TxPoWTreeNode>& zStartNode, bool zRelevant,
                           bool zCheckCoinID, const MiniData& zCoinID,
                           bool zCheckAmount, const MiniNumber& zAmount,
                           bool zCheckAddress, const MiniData& zAddress,
                           bool zCheckTokenID, const MiniData& zTokenID,
                           bool zSimpleOnly) {
    return searchCoins(zStartNode, zRelevant, zCheckCoinID, zCoinID, zCheckAmount,
                       zAmount, zCheckAddress, zAddress, zCheckTokenID, zTokenID,
                       zSimpleOnly, std::numeric_limits<int>::max());
}

std::vector<std::shared_ptr<Coin>>
TxPoWSearcher::searchCoins(const std::shared_ptr<TxPoWTreeNode>& zStartNode, bool zRelevant,
                           bool zCheckCoinID, const MiniData& zCoinID,
                           bool zCheckAmount, const MiniNumber& zAmount,
                           bool zCheckAddress, const MiniData& zAddress,
                           bool zCheckTokenID, const MiniData& zTokenID,
                           bool zSimpleOnly, int zDepth) {
    return searchCoins(zStartNode, zRelevant, zCheckCoinID, zCoinID, zCheckAmount,
                       zAmount, zCheckAddress, zAddress, zCheckTokenID, zTokenID,
                       false, std::string(), false,
                       zSimpleOnly, zDepth, false);
}

std::vector<std::shared_ptr<Coin>>
TxPoWSearcher::searchCoins(const std::shared_ptr<TxPoWTreeNode>& zStartNode, bool zRelevant,
                           bool zCheckCoinID, const MiniData& zCoinID,
                           bool zCheckAmount, const MiniNumber& zAmount,
                           bool zCheckAddress, const MiniData& zAddress,
                           bool zCheckTokenID, const MiniData& zTokenID,
                           bool zCheckState, const std::string& zState, bool zWildCardState,
                           bool zSimpleOnly, int zDepth, bool zMEGAMMR) {
    std::vector<std::shared_ptr<Coin>> coinentry;

    // Start node
    std::shared_ptr<TxPoWTreeNode> tip = zStartNode;

    // Track spent coinids
    std::unordered_set<std::string> spentcoins;

    int depth = 0;
    bool MEGACHECK = false;

    while (tip || MEGACHECK) {
        if (depth++ > zDepth) {
            break;
        }

        std::vector<const Coin*> coins;

        if (!MEGACHECK) {
            if (tip) {
                if (zRelevant) {
                    const auto& rc = tip->getRelevantCoins();
                    coins.reserve(rc.size());
                    for (const auto& sp : rc) {
                        coins.push_back(&sp);
                    }
                } else {
                    const auto& ac = tip->getAllCoins();
                    coins.reserve(ac.size());
                    for (const auto& sp : ac) {
                        coins.push_back(&sp);
                    }
                }
            }
        } else {
            // Lock DB while accessing MEGAMMR
            MinimaDB::getDB()->readLock(true);

            //
            // FIX: Get MegaMMR as a reference (auto&) to avoid deleted copy constructor
            //
            auto& mega = MinimaDB::getDB()->getMegaMMR();
            //
            // FIX: Use dot '.' operator
            //
            const auto& allmap = mega.getAllCoins(); // assumed map-like with shared_ptr<Coin> values
            coins.reserve(allmap.size());
            for (const auto& kv : allmap) {
                coins.push_back(kv.second.get());
            }
        }

        for (const auto* coin : coins) {
            if (zCheckTokenID && !coin->getTokenID().isEqual(zTokenID)) {
                continue;
            }
            if (zCheckCoinID && !coin->getCoinID().isEqual(zCoinID)) {
                continue;
            }
            if (zCheckAmount && !coin->getAmount().isEqual(zAmount)) {
                continue;
            }
            if (zCheckAddress && !coin->getAddress().isEqual(zAddress)) {
                continue;
            }
            if (zCheckState && !coin->checkForStateVariable(zState, zWildCardState)) {
                continue;
            }

            std::string coinid = coin->getCoinID().to0xString();
            bool spent = coin->getSpent();

            if (spent) {
                spentcoins.insert(coinid);
            } else {
                if (spentcoins.find(coinid) == spentcoins.end()) {
                    // Deep copy then store as shared_ptr
                    std::unique_ptr<Coin> copy = coin->deepCopy();
                    std::shared_ptr<Coin> scopy(std::move(copy));
                    coinentry.push_back(std::move(scopy));

                    // Mark to prevent future duplicates
                    spentcoins.insert(coinid);
                }
            }
        }

        if (!MEGACHECK) {
            // Move up
            if (tip) {
                tip = tip->getParent();
            }
            if (!tip && zMEGAMMR) {
                MEGACHECK = true;
            }
        } else {
            // Unlock DB after MEGAMMR access
            MinimaDB::getDB()->readLock(false);
            break; // MEGAMMR only checked once
        }
    }

    // Filter simple addresses if requested
    std::vector<std::shared_ptr<Coin>> finalcoins = coinentry;
    if (zSimpleOnly) {
        finalcoins.clear();
        //
        // FIX: Change Wallet* to Wallet&
        //
        Wallet& wallet = MinimaDB::getDB()->getWallet();
        for (const auto& cc : coinentry) {
            //
            // FIX: Use dot '.' operator
            //
            if (wallet.isAddressSimple(cc->getAddress().to0xString())) {
                finalcoins.push_back(cc);
            }
        }
    }

    return finalcoins;
}

std::shared_ptr<TxPoWTreeNode>
TxPoWSearcher::getTreeNodeForCoin(const MiniData& zCoinID) {
    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    while (tip) {
        const auto& coins = tip->getAllCoins();
        for (const auto& coin : coins) {
            if (coin.getCoinID().isEqual(zCoinID)) {
                return tip;
            }
        }
        tip = tip->getParent();
    }
    return nullptr;
}

std::shared_ptr<TxPoW>
TxPoWSearcher::getTxPoWBlock(const MiniNumber& zBlockNumber) {
    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    while (tip) {
        auto bn = tip->getBlockNumber();
        if (bn.isEqual(zBlockNumber)) {
            return std::shared_ptr<TxPoW>(tip, &(tip->getTxPoW()));
        }
        tip = tip->getParent();
    }
    return nullptr;
}

std::shared_ptr<TxPoW>
TxPoWSearcher::searchChainForTxPoW(const MiniData& zTxPoWID) {
    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    while (tip) {
        std::shared_ptr<TxPoW> txblock(tip, &(tip->getTxPoW()));

        if (txblock->getTxPoWIDData().isEqual(zTxPoWID)) {
            return txblock;
        }

        auto txns = txblock->getBlockTransactions();
        for (const auto& txn : txns) {
            if (txn.isEqual(zTxPoWID)) {
                return txblock;
            }
        }

        tip = tip->getParent();
    }
    return nullptr;
}

std::shared_ptr<TxPoWTreeNode>
TxPoWSearcher::searchChainForTxPoWBlock(const MiniData& zTxPoWID) {
    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    while (tip) {
        std::shared_ptr<TxPoW> txblock(tip, &(tip->getTxPoW()));
        if (txblock->getTxPoWIDData().isEqual(zTxPoWID)) {
            return tip;
        }
        tip = tip->getParent();
    }
    return nullptr;
}

std::vector<std::shared_ptr<TxPoW>>
TxPoWSearcher::searchTxPoWviaAddress(const MiniData& zAddress) {
    std::vector<std::shared_ptr<TxPoW>> ret;

    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    //
    // FIX: Change TxPoWDB* to TxPoWDB&
    //
    TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();

    while (tip) {
        const auto& coins = tip->getAllCoins();

        for (const auto& coin : coins) {
            if (coin.getAddress().isEqual(zAddress)) {
                std::shared_ptr<TxPoW> txblock(tip, &(tip->getTxPoW()));

                if (checkTxPoWForAddress(*txblock, zAddress)) {
                    ret.push_back(txblock);
                }

                auto txns = txblock->getBlockTransactions();
                for (const auto& txn : txns) {
                    //
                    // FIX: Use dot '.' operator
                    //
                    auto txp = txpdb.getTxPoW(txn.to0xString());
                    if (txp) {
                        if (checkTxPoWForAddress(*txp, zAddress)) {
                            ret.push_back(txp);
                        }
                    }
                }
            }
        }

        tip = tip->getParent();
    }

    return ret;
}

bool
TxPoWSearcher::checkTxPoWForAddress(const TxPoW& zTxPoW, const MiniData& zAddress) {
    // Inputs
    {
        const auto& coins = zTxPoW.getTransaction().getAllInputs();
        for (const auto& cc : coins) {
            if (cc->getAddress().isEqual(zAddress)) {
                return true;
            }
        }
    }
    {
        const auto& coins = zTxPoW.getBurnTransaction().getAllInputs();
        for (const auto& cc : coins) {
            if (cc->getAddress().isEqual(zAddress)) {
                return true;
            }
        }
    }
    // Outputs
    {
        const auto& coins = zTxPoW.getTransaction().getAllOutputs();
        for (const auto& cc : coins) {
            if (cc->getAddress().isEqual(zAddress)) {
                return true;
            }
        }
    }
    {
        const auto& coins = zTxPoW.getBurnTransaction().getAllOutputs();
        for (const auto& cc : coins) {
            if (cc->getAddress().isEqual(zAddress)) {
                return true;
            }
        }
    }
    return false;
}

bool
TxPoWSearcher::checkTxPoWRelevant(const TxPoW& zTxPoW, Wallet& zWallet) {
    // Inputs
    {
        const auto& coins = zTxPoW.getTransaction().getAllInputs();
        for (const auto& cc : coins) {
            std::string address = cc->getAddress().to0xString();
            if (zWallet.isAddressRelevant(address)) {
                return true;
            }
            const auto& state = cc->getState();
            for (const auto& svu : state) {
                const StateVariable* sv = svu.get();
                if (sv->getType().isEqual(StateVariable::STATETYPE_HEX)) {
                    std::string svstr = sv->toString();
                    if (zWallet.isAddressRelevant(svstr) || zWallet.isKeyRelevant(svstr)) {
                        return true;
                    }
                }
            }
        }
    }
    {
        const auto& coins = zTxPoW.getBurnTransaction().getAllInputs();
        for (const auto& cc : coins) {
            std::string address = cc->getAddress().to0xString();
            if (zWallet.isAddressRelevant(address)) {
                return true;
            }
            const auto& state = cc->getState();
            for (const auto& svu : state) {
                const StateVariable* sv = svu.get();
                if (sv->getType().isEqual(StateVariable::STATETYPE_HEX)) {
                    std::string svstr = sv->toString();
                    if (zWallet.isAddressRelevant(svstr) || zWallet.isKeyRelevant(svstr)) {
                        return true;
                    }
                }
            }
        }
    }
    // Outputs
    {
        const auto& coins = zTxPoW.getTransaction().getAllOutputs();
        for (const auto& cc : coins) {
            std::string address = cc->getAddress().to0xString();
            if (zWallet.isAddressRelevant(address)) {
                return true;
            }
        }
    }
    {
        const auto& coins = zTxPoW.getBurnTransaction().getAllOutputs();
        for (const auto& cc : coins) {
            std::string address = cc->getAddress().to0xString();
            if (zWallet.isAddressRelevant(address)) {
                return true;
            }
        }
    }
    // Complete state of transactions
    {
        const auto& state = zTxPoW.getTransaction().getCompleteState();
        for (const auto& svu : state) {
            const StateVariable* sv = svu.get();
            if (sv->getType().isEqual(StateVariable::STATETYPE_HEX)) {
                std::string svstr = sv->toString();
                if (zWallet.isAddressRelevant(svstr) || zWallet.isKeyRelevant(svstr)) {
                    return true;
                }
            }
        }
    }
    {
        const auto& state = zTxPoW.getBurnTransaction().getCompleteState();
        for (const auto& svu : state) {
            const StateVariable* sv = svu.get();
            if (sv->getType().isEqual(StateVariable::STATETYPE_HEX)) {
                std::string svstr = sv->toString();
                if (zWallet.isAddressRelevant(svstr) || zWallet.isKeyRelevant(svstr)) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::vector<std::shared_ptr<Token>>
TxPoWSearcher::getAllTokens() {
    std::lock_guard<std::mutex> lock(s_importedTokensMutex);

    std::vector<std::shared_ptr<Token>> tokens;
    std::unordered_set<std::string> added;

    // Traverse chain
    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    while (tip) {
        const auto& coins = tip->getAllCoins();
        for (const auto& coin : coins) {
            std::string tokenid = coin.getTokenID().to0xString();
            if (tokenid != "0x00") {
                if (added.find(tokenid) == added.end()) {
                    added.insert(tokenid);
                    const Token* rawtok = coin.getToken();
                    // Alias a shared_ptr<Token> to the Coin owner
                    tokens.emplace_back(tip, const_cast<Token*>(rawtok));
                }
            }
        }
        tip = tip->getParent();
    }

    // Include imported tokens
    for (const auto& tok : s_importedTokens) {
        const MiniData* tid = tok->getTokenID();
        if (tid) {
            std::string tokenid = tid->to0xString();
            if (added.find(tokenid) == added.end()) {
                added.insert(tokenid);
                tokens.push_back(tok);
            }
        }
    }

    return tokens;
}

void
TxPoWSearcher::importToken(const std::shared_ptr<Token>& zToken) {
    std::lock_guard<std::mutex> lock(s_importedTokensMutex);
    s_importedTokens.push_back(zToken);
}

std::shared_ptr<Token>
TxPoWSearcher::getToken(const MiniData& zTokenID) {
    std::lock_guard<std::mutex> lock(s_importedTokensMutex);

    // Search imported first
    for (const auto& tok : s_importedTokens) {
        const MiniData* tid = tok->getTokenID();
        if (tid && tid->isEqual(zTokenID)) {
            return tok;
        }
    }

    // Traverse chain and optionally MEGAMMR
    //
    // FIX: Change TxPowTree* to TxPowTree&
    //
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX: Use dot '.' operator
    //
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    bool MEGACHECK = false;
    bool locked = false;

    while (tip || MEGACHECK) {
        std::vector<std::shared_ptr<Coin>> coins;

        if (!MEGACHECK) {
            if (tip) {
                const auto& all_coins_vec = tip->getAllCoins();
                coins.clear();
                coins.reserve(all_coins_vec.size());
                for(const auto& c : all_coins_vec) {
                    coins.emplace_back(tip, const_cast<Coin*>(&c)); // Aliasing constructor
                }
            }
        } else {
            MinimaDB::getDB()->readLock(true);
            locked = true;

            //
            // FIX: Get MegaMMR as a reference (auto&)
            //
            auto& mega = MinimaDB::getDB()->getMegaMMR();
            //
            // FIX: Use dot '.' operator
            //
            const auto& allmap = mega.getAllCoins();
            coins.reserve(allmap.size());
            auto null_deleter = [](org::minima::objects::Coin*){};
            for (const auto& kv : allmap) {
                coins.emplace_back(kv.second.get(), null_deleter);
            }
        }

        for (const auto& spcoin : coins) {
            if (spcoin->getTokenID().isEqual(zTokenID)) {
                Token* rawtok = spcoin->getToken();
                std::shared_ptr<Token> ret(spcoin, rawtok);
                if (locked) {
                    MinimaDB::getDB()->readLock(false);
                    locked = false;
                }
                return ret;
            }
        }

        if (!MEGACHECK) {
            tip = tip->getParent();
            if (!tip && org::minima::system::params::GeneralParams::IS_MEGAMMR) {
                MEGACHECK = true;
            }
        } else {
            if (locked) {
                MinimaDB::getDB()->readLock(false);
                locked = false;
            }
            break;
        }
    }

    return nullptr;
}

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org
