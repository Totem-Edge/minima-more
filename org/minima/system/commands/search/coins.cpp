#include "org/minima/system/commands/search/coins.hpp"

#include <algorithm>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
// Forward declare TxPoWTree to avoid missing header dependency
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
namespace org { namespace minima { namespace database { namespace txpowtree {
class TxPoWTree {
public:
    std::shared_ptr<TxPoWTreeNode> getTip();
};
} } } }

#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/send/send.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

coins::coins()
    : org::minima::system::commands::Command(
          "coins",
          "(relevant:true) (sendable:true) (coinid:) (amount:) (address:) (tokenid:) (coinage:) (checkmempool:) (order:) - Search for coins") {}

std::string coins::getFullHelp() const {
    return "\ncoins\n"
           "\n"
           "Search for coins that are relevant to you or in the unpruned chain.\n"
           "\n"
           "relevant: (optional)\n"
           "    true or false, true will only return coins you are tracking.\n"
           "    false will search all coins in the unpruned chain.\n"
           "    Default is false unless no other parameters are provided.\n"
           "\n"
           "sendable: (optional)\n"
           "    true only, filter out coins that are not sendable, they might be locked in a contract.\n"
           "    Default is to return sendable and unsendable coins.\n"
           "\n"
           "coinid: (optional)\n"
           "    A coinid, to search for a single coin.\n"
           "\n"
           "amount: (optional)\n"
           "    The coin value to search for.\n"
           "\n"
           "address: (optional)\n"
           "    Address of a coin to search for, could be a script address.\n"
           "    Can be a 0x or Mx address.\n"
           "\n"
           "tokenid: (optional)\n"
           "    A tokenid, to search for coins of a specific token. Minima is 0x00.\n"
           "\n"
           "checkmempool: (optional)\n"
           "    Check if the coin is in the mempool.\n"
           "\n"
           "coinage: (optional)\n"
           "    How old does the coin have to be.\n"
           "\n"
           "depth: (optional)\n"
           "    How many blocks back to check from Blockchain tip.\n"
           "\n"
           "order: (optional)\n"
           "    Order asc or desc (Ascending or Decending).\n"
           "\n"
           "megammr: (optional)\n"
           "    Search the MegaMMR for coins too.\n"
           "\n"
           "Examples:\n"
           "\n"
           "coins\n"
           "\n"
           "coins relevant:true sendable:true\n"
           "\n"
           "coins relevant:true amount:10\n"
           "\n"
           "coins coinid:0xEECD7..\n"
           "\n"
           "coins relevant:true tokenid:0xFED5..\n"
           "\n"
           "coins relevant:true address:0xCEF6.. tokenid:0x00\n"
           "\n"
           "coins relevant:true address:MxABC9..\n";
}

std::vector<std::string> coins::getValidParams() {
    return {
        "relevant",    "sendable",   "coinid",   "amount",     "address",
        "tokenid",     "checkmempool","order",   "coinage",    "simplestate",
        "totalamount", "depth",      "state",    "megammr"
    };
}

std::unique_ptr<org::minima::utils::json::JSONObject> coins::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::txpowdb::TxPoWDB;
    using org::minima::database::txpowtree::TxPoWTreeNode;
    using org::minima::objects::Coin;
    using org::minima::objects::Token;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::system::brains::TxPoWSearcher;
    using org::minima::system::brains::TxPoWMiner;
    using org::minima::system::params::GeneralParams;
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    // Determine default for 'relevant' when no other filter params are specified
    bool hardsetrel = false;
    if (!existsParam("relevant") &&
        !existsParam("coinid") &&
        !existsParam("address") &&
        !existsParam("state") &&
        !existsParam("tokenid")) {
        hardsetrel = true;
    }

    bool relevant = existsParam("relevant");
    if (hardsetrel) {
        relevant = true;
    }

    bool simplestate = getBooleanParam("simplestate", false);
    bool simple      = getBooleanParam("sendable", false);

    // coinid
    bool scoinid = existsParam("coinid");
    MiniData coinid = MiniData::ZERO_TXPOWID();
    if (scoinid) {
        coinid = MiniData(getParam("coinid", "0x01"));
    }

    // amount
    bool samount = existsParam("amount");
    MiniNumber amount = MiniNumber::ZERO();
    if (samount) {
        auto num = getNumberParam("amount");
        amount = *num;
    }

    // address
    bool saddress = existsParam("address");
    MiniData address = MiniData::ZERO_TXPOWID();
    if (saddress) {
        address = MiniData(getAddressParam("address"));
    }

    // tokenid
    bool stokenid = existsParam("tokenid");
    MiniData tokenid = MiniData::ZERO_TXPOWID();
    if (stokenid) {
        tokenid = MiniData(getParam("tokenid", "0x01"));
    }

    // state
    bool sstate = existsParam("state");
    std::string statesearch = getParam("state", "");

    // coinage minimum age
    MiniNumber coinage = *(getNumberParam("coinage", MiniNumber::ZERO()));

    // max depth
    int maxdepth = getNumberParam("depth", MiniNumber::BILLION())->getAsInt();

    // Get chain tip node
    // FIX: Get a reference with auto&
    auto& tree = MinimaDB::getDB()->getTxPoWTree();
    
    // FIX: The `if (!tree)` check is now invalid as references cannot be null.
    // We remove it. The check for `!tip` below is the correct one.
    
    // FIX: Use . (dot operator) on the reference 'tree'
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();
    if (!tip) {
        JSONArray emptyarr;
        ret->put("response", emptyarr);
        return ret;
    }

    // MegaMMR flag gated by GeneralParams
    bool checkmegammr = getBooleanParam("megammr", false);
    if (checkmegammr) {
        checkmegammr = GeneralParams::IS_MEGAMMR;
    }

    // Run the search across the chain
    std::vector<std::shared_ptr<Coin>> coinslist =
        TxPoWSearcher::searchCoins(tip, relevant,
                                   scoinid, coinid,
                                   samount, amount,
                                   saddress, address,
                                   stokenid, tokenid,
                                   sstate, statesearch, true, // wildcard state
                                   simple, maxdepth, checkmegammr);

    // Filter by minimum age
    MiniNumber mincoinblock = tip->getBlockNumber().sub(coinage);
    std::vector<std::shared_ptr<Coin>> agecoins;
    agecoins.reserve(coinslist.size());
    for (const auto& relc : coinslist) {
        if (relc->getBlockCreated().isLessEqual(mincoinblock)) {
            agecoins.push_back(relc);
        }
    }
    coinslist = std::move(agecoins);

    // Optionally filter out coins used in mempool or currently being mined
    bool checkmempool = getBooleanParam("checkmempool", false);
    std::vector<std::shared_ptr<Coin>> finalcoins;
    if (checkmempool) {
        // FIX: Use & (address-of) to get a pointer from the returned reference
        TxPoWDB* txpdb = &MinimaDB::getDB()->getTxPoWDB();
        TxPoWMiner* txminer = &org::minima::system::Main::getInstance()->getTxPoWMiner();

        for (const auto& coin : coinslist) {
            if (txminer && txminer->checkForMiningCoin(coin->getCoinID().to0xString())) {
                continue;
            }
            if (txpdb && txpdb->checkMempoolCoins(coin->getCoinID())) {
                continue;
            }
            finalcoins.push_back(coin);
        }
    } else {
        finalcoins = coinslist;
    }

    // Order asc if requested
    std::string order = getParam("order", "desc");
    if (order == "asc") {
        std::sort(finalcoins.begin(), finalcoins.end(),
                  [](const std::shared_ptr<Coin>& a, const std::shared_ptr<Coin>& b) {
                      return a->getBlockCreated().isLess(b->getBlockCreated());
                  });
    }

    // If a total amount is requested, select minimum set of coins to reach that amount
    if (existsParam("totalamount")) {
        MiniNumber totalamount = *(getNumberParam("totalamount"));
        MiniNumber tokenamount = totalamount;

        if (!tokenid.isEqual(Token::TOKENID_MINIMA)) {
            if (!finalcoins.empty()) {
                const Token* tok = finalcoins.front()->getToken();
                if (tok) {
                    auto scaled = tok->getScaledMinimaAmount(totalamount);
                    tokenamount = *scaled;
                }
            }
        }

        // Convert shared_ptr coins to unique_ptr copies for selectCoins API
        std::vector<std::unique_ptr<Coin>> ucoins;
        ucoins.reserve(finalcoins.size());
        for (const auto& sc : finalcoins) {
            ucoins.emplace_back(sc->deepCopy());
        }

        std::vector<std::shared_ptr<org::minima::objects::Coin>> ucoins_shared;
        for (auto& coin : ucoins) {
            ucoins_shared.push_back(std::move(coin));
        }
        auto selected = org::minima::system::commands::send::send::selectCoins(ucoins_shared, tokenamount);

        // Map selected copies back to the original shared_ptr by CoinID
        std::vector<std::shared_ptr<Coin>> mapped;
        mapped.reserve(selected.size());
        for (const auto& uc : selected) {
            std::string id = uc->getCoinID().to0xString();
            for (const auto& sc : finalcoins) {
                if (sc->getCoinID().to0xString() == id) {
                    mapped.push_back(sc);
                    break;
                }
            }
        }
        finalcoins = std::move(mapped);
    }

    // Current tip block for age calculation
    MiniNumber cblock = tip->getBlockNumber();

    // Build response array
    org::minima::utils::json::JSONArray coinarr;
    for (const auto& cc : finalcoins) {
        org::minima::utils::json::JSONObject jsoncoin = cc->toJSON(simplestate);
        std::string age = cblock.sub(cc->getBlockCreated()).toString();
        jsoncoin.put("age", age);
        coinarr.add(jsoncoin);
    }

    ret->put("response", coinarr);
    return ret;
}

org::minima::system::commands::Command* coins::getFunction() {
    return new coins();
}

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
