#include "org/minima/system/commands/txn/txnaddamount.hpp"

#include <algorithm>
#include <utility>
#include <memory>
#include <limits>

#include "org/minima/system/commands/command_exception.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"

#include "org/minima/system/commands/send/send.hpp"
#include "org/minima/system/params/general_params.hpp"

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

// Static member definitions
std::vector<std::string> txnaddamount::s_LOCKED_COINS;
bool txnaddamount::s_LOCKED_COINS_ENABLED = false;

txnaddamount::txnaddamount()
    : org::minima::system::commands::Command(
          "txnaddamount",
          "[id:] [amount:] (address) (onlychange:) (tokenid:) (burn:) - Add inputs and calculate change for a certain amount") {}

std::string txnaddamount::getFullHelp() const {
    return "\ttxnaddamount\n"
           "\n"
           "Add a certain amount to a transaction.\n"
           "\n"
           "Use in conjunction with txncoinlock to create multiple offline transactions.\n"
           "\n"
           "Output amount to an address OR only the change - if you have already added an output\n"
           "\n";
}

std::vector<std::string> txnaddamount::getValidParams() {
    return std::vector<std::string>{
        "id","amount","address","onlychange","tokenid","fromaddress","burn","storestate"
    };
}

// Static coin lock management
void txnaddamount::enableCoinLock(bool zCoinLockEnabled) {
    s_LOCKED_COINS_ENABLED = zCoinLockEnabled;
    if (!s_LOCKED_COINS_ENABLED) {
        clearCoinLocked();
    }
}

bool txnaddamount::isCoinLockEnabled() {
    return s_LOCKED_COINS_ENABLED;
}

void txnaddamount::addCoinLock(const std::string& zCoinID) {
    if (s_LOCKED_COINS_ENABLED) {
        if (!isCoinLocked(zCoinID)) {
            s_LOCKED_COINS.push_back(zCoinID);
        }
    }
}

bool txnaddamount::isCoinLocked(const std::string& zCoinID) {
    if (!s_LOCKED_COINS_ENABLED) {
        return false;
    }
    return std::find(s_LOCKED_COINS.begin(), s_LOCKED_COINS.end(), zCoinID) != s_LOCKED_COINS.end();
}

void txnaddamount::clearCoinLocked() {
    s_LOCKED_COINS.clear();
}

static std::unique_ptr<org::minima::objects::Token>
cloneTokenForCoin(const org::minima::objects::Token& tok) {
    using namespace org::minima::objects::base;
    using org::minima::utils::Streamable;
    // Serialize then deserialize to deep copy
    Streamable& streamRef = const_cast<org::minima::objects::Token&>(tok);
    std::unique_ptr<MiniData> md = MiniData::getMiniDataVersion(streamRef);
    if (!md) {
        return nullptr;
    }
    return org::minima::objects::Token::convertMiniDataVersion(*md);
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnaddamount::runCommand() {
    using namespace org::minima::database;
    using namespace org::minima::database::userprefs::txndb;
    using namespace org::minima::database::txpowtree;
    using namespace org::minima::objects;
    using namespace org::minima::objects::base;
    using namespace org::minima::system::brains;
    // using namespace org::minima::system::commands::send;
    using namespace org::minima::system::params;
    using namespace org::minima::utils::json;

    auto ret = getJSONReply();

    // DB and transaction row
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    const std::string id = getParam("id");

    // Required amount
    std::unique_ptr<MiniNumber> amount_up = getNumberParam("amount");
    if (!amount_up) {
        throw org::minima::system::commands::CommandException("Invalid or missing amount");
    }
    MiniNumber amount = *amount_up;

    // Token handling
    MiniData tokenid = Token::TOKENID_MINIMA;
    std::shared_ptr<Token> token;
    if (existsParam("tokenid")) {
        std::unique_ptr<MiniData> tokid = getDataParam("tokenid");
        if (!tokid) {
            throw org::minima::system::commands::CommandException("Invalid tokenid parameter");
        }
        tokenid = *tokid;

        if (!tokenid.isEqual(Token::TOKENID_MINIMA)) {
            token = TxPoWSearcher::getToken(tokenid);
            if (!token) {
                throw org::minima::system::commands::CommandException(
                    std::string("Token not found : ") + tokenid.toString());
            }
        }
    }

    // Burn
    std::unique_ptr<MiniNumber> burn_up = getNumberParam("burn", MiniNumber::ZERO());
    MiniNumber burn = burn_up ? *burn_up : MiniNumber::ZERO();
    if (burn.isMore(MiniNumber::ZERO()) && !tokenid.isEqual(Token::TOKENID_MINIMA)) {
        throw org::minima::system::commands::CommandException(
            "Currently BURN on precreated transactions only works for Minima.. tokenid:0x00.. not tokens.");
    }

    // Amount in minima (if token specified convert token amount to minima) - not used later but kept for parity
    MiniNumber miniamount = amount;
    if (token) {
        std::unique_ptr<MiniNumber> conv = token->getScaledMinimaAmount(amount);
        if (!conv) {
            throw org::minima::system::commands::CommandException("Token scaling failed (minima)");
        }
        miniamount = *conv;
    }

    // Get transaction
    TxnRow* txnrow = db.getTransactionRow(id);
    if (!txnrow) {
        throw org::minima::system::commands::CommandException(std::string("Transaction not found : ") + id);
    }
    Transaction& trans = txnrow->getTransaction();

    // Tip node
    auto& tree = MinimaDB::getDB()->getTxPoWTree();
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();
    if (!tip) {
        // No chain tip yet
        ret->put("response", JSONArray());
        return ret;
    }

    // Convert to token amount (in minima units when tokenid != minima)
    MiniNumber tokenamount = amount;
    if (!tokenid.isEqual(Token::TOKENID_MINIMA)) {
        std::unique_ptr<MiniNumber> conv = token->getScaledMinimaAmount(amount);
        if (!conv) {
            throw org::minima::system::commands::CommandException("Token scaling failed (selection)");
        }
        tokenamount = *conv;
    }

    // Params
    bool addonlychange = getBooleanParam("onlychange", false);

    bool useaddress = false;
    MiniData fromaddress("0x00");
    if (existsParam("fromaddress")) {
        useaddress = true;
        const std::string faddr = getAddressParam("fromaddress");
        fromaddress = MiniData(faddr);
    }

    // Obtain coins
    std::vector<std::shared_ptr<Coin>> coins;
    if (!useaddress) {
        coins = TxPoWSearcher::searchCoins(
            tip,
            true,                               // relevant
            false, MiniData::ZERO_TXPOWID(),      // checkCoinID
            false, MiniNumber::ZERO(),            // checkAmount
            false, MiniData::ZERO_TXPOWID(),      // checkAddress
            true, tokenid,                      // checkTokenID
            true                                // simpleOnly
        );
    } else {
        coins = TxPoWSearcher::searchCoins(
            tip,
            false,                              // relevant
            false, MiniData::ZERO_TXPOWID(),      // checkCoinID
            false, MiniNumber::ZERO(),            // checkAmount
            true, fromaddress,                  // checkAddress
            true, tokenid,                      // checkTokenID
            false, std::string(""), true,       // checkState, state, wildcard
            false,                              // simpleOnly
            std::numeric_limits<int>::max(),    // depth
            GeneralParams::IS_MEGAMMR           // MEGAMMR
        );
    }

    // Remove locked coins if enabled
    if (isCoinLockEnabled()) {
        std::vector<std::shared_ptr<Coin>> validcoins;
        validcoins.reserve(coins.size());
        for (const auto& cc : coins) {
            if (!cc) continue;
            if (!isCoinLocked(cc->getCoinID().to0xString())) {
                validcoins.push_back(cc);
            }
        }
        coins.swap(validcoins);
    }

    // Convert to unique_ptr coins for selection
    std::vector<std::unique_ptr<Coin>> ucoins;
    ucoins.reserve(coins.size());
    for (const auto& sc : coins) {
        if (!sc) continue;
        std::unique_ptr<Coin> copy = sc->deepCopy();
        if (copy) {
            ucoins.push_back(std::move(copy));
        }
    }

    // Amount plus burn
    MiniNumber amountplusburn = tokenamount.add(burn);

    // Select coins to cover amount+burn
    std::vector<std::shared_ptr<org::minima::objects::Coin>> ucoins_shared;
    for (auto& coin : ucoins) {
        ucoins_shared.push_back(std::move(coin));
    }
    std::vector<std::shared_ptr<Coin>> finalcoins = org::minima::system::commands::send::send::selectCoins(ucoins_shared, amountplusburn);

    // Total added
    MiniNumber totaladded = MiniNumber::ZERO();
    for (const auto& cc : finalcoins) {
        if (cc) {
            totaladded = totaladded.add(cc->getAmount());
        }
    }

    // Change = totaladded - (amount + burn)
    MiniNumber change = totaladded.sub(amountplusburn);

    // Check funds
    if (change.isLess(MiniNumber::ZERO())) {
        MiniNumber total = totaladded;
        if (!tokenid.isEqual(Token::TOKENID_MINIMA) && token) {
            std::unique_ptr<MiniNumber> t = token->getScaledTokenAmount(total);
            if (t) {
                total = *t;
            }
        }
        throw org::minima::system::commands::CommandException(
            std::string("Not enough funds! Current balance : ") + total.toString());
    }

    // Add selected inputs and apply coin locks
    for (auto& cc : finalcoins) {
        if (!cc) continue;
        // Add lock if enabled BEFORE moving
        if (isCoinLockEnabled()) {
            addCoinLock(cc->getCoinID().to0xString());
        }
        trans.addInput(cc->deepCopy());
    }

    // Store state flag (for outputs)
    bool storestate = getBooleanParam("storestate", true);

    // Add main output unless only-change specified
    if (!addonlychange) {
        const std::string addr = getAddressParam("address");
        auto maincoin = std::make_unique<Coin>(MiniData(addr), tokenamount, tokenid, storestate);

        if (!tokenid.isEqual(Token::TOKENID_MINIMA) && token) {
            std::unique_ptr<Token> tclone = cloneTokenForCoin(*token);
            if (tclone) {
                maincoin->setToken(std::move(tclone));
            }
        }

        trans.addOutput(std::move(maincoin));
    }

    // Add change output if needed
    if (change.isMore(MiniNumber::ZERO())) {
        // Determine change address
        MiniData chgaddress("0x00");
        if (useaddress) {
            chgaddress = fromaddress;
        } else {
            // Get default wallet address
            auto& wallet = MinimaDB::getDB()->getWallet();

            // getDefaultAddress() returns a unique_ptr, must check and dereference it
            auto newwalletaddress_ptr = wallet.getDefaultAddress();
            if (!newwalletaddress_ptr) {
                throw org::minima::system::commands::CommandException("Failed to get default wallet address (null)");
            }

            // Now dereference the pointer (*) to get the object and copy it
            auto newwalletaddress = *newwalletaddress_ptr;
            chgaddress = MiniData(newwalletaddress.getAddress());
        }

        // Change coin does not keep the state
        auto changecoin = std::make_unique<Coin>(Coin::COINID_OUTPUT, chgaddress, change, tokenid, false);
        if (!tokenid.isEqual(Token::TOKENID_MINIMA) && token) {
            std::unique_ptr<Token> tclone = cloneTokenForCoin(*token);
            if (tclone) {
                changecoin->setToken(std::move(tclone));
            }
        }

        trans.addOutput(std::move(changecoin));
    }

    // Precompute output CoinIDs from first input and calc txid
    TxPoWGenerator::precomputeTransactionCoinID(trans);
    trans.calculateTransactionID();

    // Response: the current transaction JSON
    TxnRow* rr = db.getTransactionRow(id);
    if (!rr) {
        throw org::minima::system::commands::CommandException(std::string("Transaction not found : ") + id);
    }
    ret->put("response", rr->toJSON());

    return ret;
}

org::minima::system::commands::Command* txnaddamount::getFunction() {
    return new txnaddamount();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
