#include "org/minima/system/commands/base/balance.hpp"

#include <algorithm>
#include <unordered_map>
#include <limits>
#include <cctype>
#include <any>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/wallet/wallet.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::database::MinimaDB;
using org::minima::database::txpowtree::TxPowTree;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::wallet::Wallet;

using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;

using org::minima::system::brains::TxPoWSearcher;
using org::minima::system::commands::CommandException;

using org::minima::system::params::GeneralParams;
using org::minima::system::params::GlobalParams;

using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::parser::JSONParser;

balance::balance()
: org::minima::system::commands::Command(
      "balance",
      "(address:) (tokenid:) (confirmations:) (megammr:) - Show your total balance of Minima and tokens") {}

std::string balance::getFullHelp() const {
    return std::string("\nbalance\n")
        + "\n"
        + "Show your total balance of Minima and tokens.\n"
        + "\n"
        + "address: (optional)\n"
        + "    Show the balance for a specific 0x or Mx address.\n"
        + "\n"
        + "tokenid: (optional)\n"
        + "    Show the balance for a specific tokenid. Minima is 0x00.\n"
        + "\n"
        + "tokendetails: (optional)\n"
        + "    true or false, show the complete details for the tokens\n"
        + "\n"
        + "confirmations: (optional)\n"
        + "    Set the number of block confirmations required before a coin is considered confirmed in your balance. Default is 3.\n"
        + "\n"
        + "megammr: (optional)\n"
        + "    Search the MegaMMR for coins too.\n"
        + "\n"
        + "simple: (optional)\n"
        + "    true or flase - show a much simpler view.. just the name and confirmed amount.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "balance\n"
        + "\n"
        + "balance simple:true\n"
        + "\n"
        + "balance tokenid:0xFED5.. confirmations:10\n"
        + "\n"
        + "balance address:0xFF..\n";
}

std::vector<std::string> balance::getValidParams() const {
    return std::vector<std::string>{
        "address","tokenid","confirmations","tokendetails","megammr","simple"
    };
}

std::unique_ptr<JSONObject> balance::runCommand() {
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Params
    std::string address = getAddressParam("address", "");
    std::unique_ptr<MiniNumber> confirmations_ptr =
        getNumberParam("confirmations", GlobalParams::MINIMA_CONFIRM_DEPTH);
    MiniNumber confirmations = *confirmations_ptr;

    bool debug = getBooleanParam("debug", false);
    bool simpleview = getBooleanParam("simple", false);

    std::string onlytokenid = getParam("tokenid", "");

    //
    // Access tree as a reference
    //
    TxPowTree& txptree = MinimaDB::getDB()->getTxPoWTree();
    //
    // FIX 1: Removed '!txptree' check (references cannot be null)
    //
    if (!txptree.getTip()) {
        throw CommandException("No blocks yet..");
    }

    // Accumulators
    JSONArray balancearr;

    // Containers
    std::vector<std::string> alltokens;
    std::unordered_map<std::string, const Token*> tokens;
    std::unordered_map<std::string, MiniNumber> confirmed_map;
    std::unordered_map<std::string, MiniNumber> unconfirmed_map;
    std::unordered_map<std::string, MiniNumber> sendable_map;
    std::unordered_map<std::string, MiniNumber> totalcoins_map;

    //
    // Wallet as a reference
    // FIX 2: Changed '.' to '='
    //
    Wallet& walletdb = MinimaDB::getDB()->getWallet();

    // MegaMMR?
    bool checkmegammr = getBooleanParam("megammr", false);
    if (checkmegammr) {
        checkmegammr = GeneralParams::IS_MEGAMMR;
    }

    // Collect coins
    std::vector<std::shared_ptr<Coin>> coins;
    if (address.empty()) {
        //
        // Use dot . operator on reference
        //
        coins = TxPoWSearcher::getAllRelevantUnspentCoins(txptree.getTip());
    } else {
        coins = TxPoWSearcher::searchCoins(
            //
            // Use dot . operator on reference
            //
            txptree.getTip(),
            /*zRelevant*/ false,
            /*zCheckCoinID*/ false, MiniData::ZERO_TXPOWID(),
            /*zCheckAmount*/ false, MiniNumber::ZERO(),
            /*zCheckAddress*/ true, MiniData(address),
            /*zCheckTokenID*/ false, MiniData::ZERO_TXPOWID(),
            /*zCheckState*/ false, std::string(""), /*zWildCardState*/ true,
            /*zSimpleOnly*/ false,
            /*zDepth*/ std::numeric_limits<int>::max(),
            /*zMEGAMMR*/ checkmegammr);
    }

    // MinimaLogger::log(std::string("DEBUG [balance]: TxPoWSearcher returned ") + std::to_string(coins.size()) + " coins.");

    //
    // Top block
    // Use dot . operator on reference
    //
    std::shared_ptr<TxPoWTreeNode> tip = txptree.getTip();
    MiniNumber topblock = tip->getBlockNumber();

    // Always include Minima
    const std::string minima0x = Token::TOKENID_MINIMA.to0xString();
    alltokens.push_back(minima0x);
    totalcoins_map[minima0x] = MiniNumber::ZERO();

    if (debug) {
        MinimaLogger::log("List of found relevant coins");
    }

    // Iterate coins
    for (const std::shared_ptr<Coin>& cptr : coins) {
        if (!cptr) continue;
        const Coin& coin = *cptr;

        if (!address.empty()) {
            if (coin.getAddress().to0xString() != address) {
                continue;
            }
        }

        if (!onlytokenid.empty()) {
            if (coin.getTokenID().to0xString() != onlytokenid) {
                continue;
            }
        }

        if (debug) {
            MinimaLogger::log(std::string("Coin : ") + coin.toJSON().toString());
        }

        // Value
        MiniNumber amount = coin.getAmount();

        // Token ID
        std::string tokenid = coin.getTokenID().to0xString();
        if (std::find(alltokens.begin(), alltokens.end(), tokenid) == alltokens.end()) {
            alltokens.push_back(tokenid);
            const Token* tok = coin.getToken();
            tokens[tokenid] = tok;
        }

        // Depth
        MiniNumber depth = topblock.sub(coin.getBlockCreated());

        // Which bucket
        bool isconfirmed = true;
        std::unordered_map<std::string, MiniNumber>* current = &confirmed_map;
        if (depth.isLess(confirmations)) {
            current = &unconfirmed_map;
            isconfirmed = false;
        }

        auto itcur = current->find(tokenid);
        if (itcur == current->end()) {
            (*current)[tokenid] = amount;
        } else {
            itcur->second = itcur->second.add(amount);
        }

        // Count coins
        auto itcnt = totalcoins_map.find(tokenid);
        if (itcnt == totalcoins_map.end()) {
            totalcoins_map[tokenid] = MiniNumber::ONE();
        } else {
            itcnt->second = itcnt->second.increment();
        }

        // Sendable diagnostics
        if (debug) {
            //
            // FIX 3: walletdb is now in scope
            //
            bool simple = walletdb.isAddressSimple(coin.getAddress().to0xString());
            if (!simple) {
                MinimaLogger::log(std::string("NON-SENDABLE : ") + coin.toJSON().toString());
            } else {
                MinimaLogger::log(std::string("SENDABLE : ") + coin.toJSON().toString());
            }
        }

        // Sendable sums
        if (tokenid == "0x00") {
            //
            // FIX 3: walletdb is now in scope
            //
            if (isconfirmed && walletdb.isAddressSimple(coin.getAddress().to0xString())) {
                auto itsen = sendable_map.find(tokenid);
                if (itsen == sendable_map.end()) {
                    sendable_map[tokenid] = amount;
                } else {
                    itsen->second = itsen->second.add(amount);
                }
            }
        } else {
            const Token* tok = coin.getToken();
            std::string script = tok ? tok->getTokenScript().toString() : "";
            if (script == "RETURN TRUE") {
                //
                // FIX 3: walletdb is now in scope
                //
                if (isconfirmed && walletdb.isAddressSimple(coin.getAddress().to0xString())) {
                    auto itsen = sendable_map.find(tokenid);
                    if (itsen == sendable_map.end()) {
                        sendable_map[tokenid] = amount;
                    } else {
                        itsen->second = itsen->second.add(amount);
                    }
                }
            }
        }
    }

    // Token details?
    bool tokendetails = getBooleanParam("tokendetails", false);

    // Build per-token JSON
    for (const std::string& token : alltokens) {
        auto it_unconf = unconfirmed_map.find(token);
        MiniNumber unconf = (it_unconf == unconfirmed_map.end()) ? MiniNumber::ZERO() : it_unconf->second;

        auto it_conf = confirmed_map.find(token);
        MiniNumber conf = (it_conf == confirmed_map.end()) ? MiniNumber::ZERO() : it_conf->second;

        auto it_send = sendable_map.find(token);
        MiniNumber send = (it_send == sendable_map.end()) ? MiniNumber::ZERO() : it_send->second;

        auto it_tcoins = totalcoins_map.find(token);
        MiniNumber totcoins = (it_tcoins == totalcoins_map.end()) ? MiniNumber::ZERO() : it_tcoins->second;

        JSONObject tokbal;

        if (token == "0x00") {
            if (onlytokenid.empty() || onlytokenid == "0x00") {
                tokbal.put("token", std::string("Minima"));
                tokbal.put("tokenid", token);
                tokbal.put("confirmed", conf.toString());
                tokbal.put("unconfirmed", unconf.toString());
                tokbal.put("sendable", send.toString());
                tokbal.put("coins", totcoins.toString());
                tokbal.put("total", std::string("1000000000"));
                balancearr.add(tokbal);
            }
        } else {
            const Token* tok = nullptr;
            auto ittok = tokens.find(token);
            if (ittok != tokens.end()) {
                tok = ittok->second;
            }

            if (tok) {
                // Store the token name as a string (C++ JSONObject serialises std::string;
                // this matches Java's tok.getName().toString() output).
                std::string tname = tok->getName().toString();
                tokbal.put("token", tname);

                tokbal.put("tokenid", token);
                tokbal.put("confirmed", tok->getScaledTokenAmount(conf)->toString());
                tokbal.put("unconfirmed", tok->getScaledTokenAmount(unconf)->toString());
                tokbal.put("sendable", tok->getScaledTokenAmount(send)->toString());
                tokbal.put("coins", totcoins.toString());
                tokbal.put("total", tok->getTotalTokens()->toString());

                if (tokendetails) {
                    JSONObject tdetails;
                    tdetails.put("decimals", tok->getDecimalPlaces()->toString());
                    tdetails.put("script", tok->getTokenScript().toString());
                    tdetails.put("totalamount", tok->getAmount().toString());
                    tdetails.put("scale", tok->getScale().toString());
                    tdetails.put("created", tok->getCreated().toString());

                    tokbal.put("details", tdetails);
                }

                balancearr.add(tokbal);
            }
        }
    }

    // Simple view?
    if (simpleview) {
        JSONArray simplebalance;
        const std::vector<std::any>& elems = balancearr.elements();
        for (const std::any& item : elems) {
            const JSONObject& oldbal = std::any_cast<const JSONObject&>(item);

            JSONObject newbal;

            std::string tokid = oldbal.getString("tokenid");
            if (tokid == "0x00") {
                newbal.put("name", std::string("Minima"));
            } else {
                std::string tokstr = oldbal.getString("token");
                JSONParser parser;
                std::any parsed = parser.parse(tokstr);
                // parser may return shared_ptr<JSONObject> or other types
                if (parsed.type() == typeid(std::shared_ptr<JSONObject>)) {
                    auto tokobj = std::any_cast<std::shared_ptr<JSONObject>>(parsed);
                    if (tokobj && tokobj->containsKey("name")) {
                        newbal.put("name", tokobj->get("name"));
                    } else {
                        newbal.put("name", tokstr);
                    }
                } else {
                    newbal.put("name", tokstr);
                }
            }

            newbal.put("amount", oldbal.getString("confirmed"));
            simplebalance.add(newbal);
        }

        ret->put("response", simplebalance);
    } else {
        ret->put("response", balancearr);
    }

    return ret;
}

org::minima::system::commands::Command* balance::getFunction() {
    return new balance();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org