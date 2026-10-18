#include "org/minima/system/commands/base/tokencreate.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sstream>

// CommandException
#include "org/minima/system/commands/command_exception.hpp"

// DB and chain
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

// Wallet
#include "org/minima/database/wallet/wallet.hpp"

// Objects
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

// System/Brains
#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/params/global_params.hpp"

// JSON
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::wallet::Wallet;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::ScriptProof;
using org::minima::objects::StateVariable;
using org::minima::objects::Token;
using org::minima::objects::Transaction;
using org::minima::objects::TxPoW;
using org::minima::objects::Witness;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::objects::keys::Signature;
using org::minima::objects::mmr::MMRProof;
using org::minima::system::Main;
using org::minima::system::brains::TxPoWGenerator;
using org::minima::system::brains::TxPoWMiner;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::system::params::GlobalParams;
using org::minima::utils::json::JSONObject;

namespace {

// Trim helpers
static inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static inline std::string unquote(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
        std::string out;
        out.reserve(t.size() - 2);
        bool esc = false;
        for (size_t i = 1; i + 1 < t.size(); ++i) {
            char c = t[i];
            if (esc) {
                out.push_back(c);
                esc = false;
            } else if (c == '\\') {
                esc = true;
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
    return t;
}

// Parse a simple flat JSON object like {"1":"value","2":"value2"} into vector of (key,value)
static std::vector<std::pair<std::string, std::string>> parseSimpleObjectKV(const std::string& json) {
    std::vector<std::pair<std::string, std::string>> res;
    std::string s = trim(json);
    if (s.empty()) return res;
    if (s.front() != '{' || s.back() != '}') return res;
    // Strip outer braces
    s = s.substr(1, s.size() - 2);

    // Split by commas at top level (not inside quotes)
    std::vector<std::string> entries;
    std::string cur;
    bool in_str = false;
    bool esc = false;
    for (char c : s) {
        if (in_str) {
            cur.push_back(c);
            if (esc) {
                esc = false;
            } else if (c == '\\') {
                esc = true;
            } else if (c == '"') {
                in_str = false;
            }
        } else {
            if (c == '"') {
                in_str = true;
                cur.push_back(c);
            } else if (c == ',') {
                entries.push_back(trim(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
    }
    if (!cur.empty()) entries.push_back(trim(cur));

    for (const auto& ent : entries) {
        if (ent.empty()) continue;
        // split on first colon not inside quotes
        bool in2 = false;
        bool esc2 = false;
        size_t idx = std::string::npos;
        for (size_t i = 0; i < ent.size(); ++i) {
            char c = ent[i];
            if (in2) {
                if (esc2) esc2 = false;
                else if (c == '\\') esc2 = true;
                else if (c == '"') in2 = false;
            } else {
                if (c == '"') in2 = true;
                else if (c == ':') { idx = i; break; }
            }
        }
        if (idx == std::string::npos) continue;
        std::string k = trim(ent.substr(0, idx));
        std::string v = trim(ent.substr(idx + 1));
        k = unquote(k);
        v = unquote(v);
        if (!k.empty()) {
            res.emplace_back(k, v);
        }
    }
    return res;
}

} // anonymous namespace

tokencreate::tokencreate()
    : org::minima::system::commands::Command(
          "tokencreate",
          "[name:] [amount:] (decimals:) (script:) (state:{}) (signtoken:) (webvalidate:) (burn:) - Create a token. 'name' can be a JSON Object") {}

std::string tokencreate::getFullHelp() const {
    return std::string("\ntokencreate\n"
                       "\n"
                       "Create (mint) custom tokens or NFTs.\n"
                       "\n"
                       "You must have some sendable Minima in your wallet as tokens are 'colored coins', a fraction of 1 Minima.\n"
                       "\n"
                       "name:\n"
                       "    The name of the token. Can be a string or JSON Object.\n"
                       "\n"
                       "amount: \n"
                       "    The amount of total supply to create for the token. Between 1 and 1 Trillion.\n"
                       "\n"
                       "decimals: (optional)\n"
                       "    The number of decimal places for the token. Default is 8, maximum 16.\n"
                       "    To create NFTs, use 0.\n"
                       "\n"
                       "script: (optional)\n"
                       "    Add a custom script that must return 'TRUE' when spending any coin of this token.\n"
                       "    Both the token script and coin script must return 'TRUE' for a coin to be sendable.\n"
                       "\n"
                       "state: (optional)\n"
                       "    List of state variables, if adding a script. A JSON object in the format {\"port\":\"value\",..}\n"
                       "\n"
                       "signtoken: (optional)\n"
                       "    Provide a public key to sign the token with.\n"
                       "    Useful for proving you are the creator of the token/NFT.\n"
                       "\n"
                       "webvalidate: (optional)\n"
                       "    Provide a URL to a publicly viewable .txt file you are hosting which stores the tokenid for validation purposes.\n"
                       "    Create the file in advance and get the tokenid after the token has been minted.\n"
                       "\n"
                       "burn: (optional)\n"
                       "    Amount to burn with the tokencreate minting transaction.\n"
                       "\n"
                       "mine: (optional)\n"
                       "    Mine the TxPoW synchronously.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "tokencreate name:newtoken amount:1000000\n"
                       "\n"
                       "tokencreate amount:10 name:{\"name\":\"newcoin\",\"link\":\"http:mysite.com\",\"description\":\"A very cool token\"}\n"
                       "\n"
                       "tokencreate name:mynft amount:10 decimals:0 webvalidate:https://www.mysite.com/nftvalidation.txt signtoken:0xFF.. burn:0.1\n"
                       "\n"
                       "tokencreate name:charitycoin amount:1000 script:\"ASSERT VERIFYOUT(@TOTOUT-1 0xMyAddress 1 0x00 TRUE)\"\n");
}

std::vector<std::string> tokencreate::getValidParams() const {
    return std::vector<std::string>{"name", "amount", "decimals", "script",
                                    "state", "signtoken", "webvalidate", "burn", "mine", "uselimits"};
}

std::unique_ptr<JSONObject> tokencreate::runCommand() {
    auto ret = getJSONReply();

    try {
        // Check the basics
        if (!existsParam("name") || !existsParam("amount")) {
            throw CommandException("MUST specify name and amount");
        }

        // Are we Mining synchronously
        bool minesync = getBooleanParam("mine", false);

        // Are we adding limits to the Number of Tokens allowed
        bool uselimits = getBooleanParam("uselimits", true);

        // Is there a state JSON
        std::unique_ptr<JSONObject> state_obj = std::make_unique<JSONObject>();
        if (existsParam("state")) {
            state_obj = getJSONObjectParam("state");
        }

        // Is name a JSON
        std::unique_ptr<JSONObject> jsonname;
        if (isParamJSONObject("name")) {
            // Get the JSON
            jsonname = getJSONObjectParam("name");

            // make sure there is a name object
            if (!jsonname->containsKey("name")) {
                throw CommandException("MUST specify a 'name' for the token in the JSON");
            }

        } else {
            // It's a String.. create a JSON
            jsonname = std::make_unique<JSONObject>();
            jsonname->put("name", getParam("name"));
        }

        // The amount is always a string
        std::string amount = getParam("amount");

        // The burn
        MiniNumber burn = *getNumberParam("burn", MiniNumber::ZERO());

        // How many decimals - can be 0.. for an NFT
        int decimals = 8;
        if (existsParam("decimals")) {
            std::string decimals_str = getParam("decimals");
            try {
                decimals = std::stoi(decimals_str);
            } catch (...) {
                throw CommandException("Invalid decimals value");
            }

            // Safety check.. not consensus set - could be more.
            if (uselimits && decimals > 16) {
                throw CommandException("MAX 16 decimal places");
            }
        }

        std::string script = "RETURN TRUE";
        if (existsParam("script")) {
            script = getParam("script");
        }

        // Now construct the txn
        if (!jsonname || amount.empty()) {
            throw CommandException("MUST specify name and amount");
        }

        // The actual amount of tokens
        MiniNumber totaltoks(amount);
        totaltoks = totaltoks.floor();

        // Safety check Amount is within tolerant levels.. could use ALL their Minima otherwise
        if (uselimits && totaltoks.isMore(MiniNumber::TRILLION())) {
            throw CommandException("MAX 1 Trillion coins for a token");
        }

        if (totaltoks.isLessEqual(MiniNumber::ZERO())) {
            throw CommandException("Cannot create less than 1 token");
        }

        // Decimals as a number
        MiniNumber totaldecs = MiniNumber::TEN().pow(decimals);

        // How much Minima will it take to colour
        MiniNumber colorminima = MiniNumber::MINI_UNIT().mult(totaldecs).mult(totaltoks);

        // What is the scale
        int scale = MiniNumber::MAX_DECIMAL_PLACES - decimals;

        // The actual amount of Minima that needs to be sent - add the burn if any
        MiniNumber sendamount = colorminima.add(burn);

        // Send it to ourselves
        auto sendkey = MinimaDB::getDB()->getWallet().getDefaultAddress();
        MiniData sendaddress(sendkey->getAddress());

        // get the tip
        auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

        // Lets build a transaction
        std::vector<std::shared_ptr<Coin>> foundcoins = TxPoWSearcher::getRelevantUnspentCoins(tip, "0x00", true);
        std::vector<std::shared_ptr<Coin>> relcoins;

        // Now make sure they are old enough
        MiniNumber mincoinblock = tip->getBlockNumber().sub(GlobalParams::MINIMA_CONFIRM_DEPTH);
        for (const auto& relc : foundcoins) {
            if (relc->getBlockCreated().isLessEqual(mincoinblock)) {
                relcoins.push_back(relc);
            }
        }

        // Are there any coins at all
        if (relcoins.empty()) {
            throw CommandException("No Minima Coins available!");
        }

        // The current total
        MiniNumber currentamount = MiniNumber::ZERO();
        std::vector<std::shared_ptr<Coin>> currentcoins;

        // Get the TxPoWDB
        TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
        TxPoWMiner& txminer = Main::getInstance()->getTxPoWMiner();

        // Now cycle through
        for (const auto& coin : relcoins) {
            // Check if we are already using them in another Transaction that is being mined
            if (txminer.checkForMiningCoin(coin->getCoinID().to0xString())) {
                continue;
            }

            // Check if in mempool
            if (txpdb.checkMempoolCoins(coin->getCoinID())) {
                continue;
            }

            // Add this coin
            currentcoins.push_back(coin);
            currentamount = currentamount.add(coin->getAmount());

            // Do we have enough
            if (currentamount.isMoreEqual(sendamount)) {
                break;
            }
        }

        // Did we add enough
        if (currentamount.isLess(sendamount)) {
            // Not enough funds
            ret->put("status", false);
            ret->put("message", std::string("Insufficient funds.. you only have ") + currentamount.toString());
            return ret;
        }

        // What is the change
        MiniNumber change = currentamount.sub(sendamount);

        // Lets construct a txn
        Transaction transaction;
        Witness witness;

        // Min depth of a coin
        MiniNumber minblock = MiniNumber::ZERO();

        // Add the inputs
        for (const auto& inputs : currentcoins) {
            // Add this input to our transaction
            transaction.addInput(inputs->deepCopy());

            // How deep
            if (inputs->getBlockCreated().isMore(minblock)) {
                minblock = inputs->getBlockCreated();
            }
        }

        // Get the block
        MiniNumber currentblock = tip->getBlockNumber();
        MiniNumber blockdiff = currentblock.sub(minblock);
        if (blockdiff.isMore(GlobalParams::MINIMA_MMR_PROOF_HISTORY)) {
            blockdiff = GlobalParams::MINIMA_MMR_PROOF_HISTORY;
        }

        // Now get that Block
        std::shared_ptr<TxPoWTreeNode> mmrnode = tip->getPastNode(tip->getBlockNumber().sub(blockdiff));
        if (!mmrnode) {
            // Not enough blocks
            throw CommandException("Not enough blocks in chain to make valid MMR Proofs..");
        }

        // Get the main Wallet
        Wallet& walletdb = MinimaDB::getDB()->getWallet();

        // Create a list of the required signatures
        std::vector<std::string> reqsigs;

        // Which Coins are added
        std::vector<std::string> addedcoinid;

        // Add the MMR proofs for the coins
        for (const auto& input : currentcoins) {
            // Keep for burn calc
            addedcoinid.push_back(input->getCoinID().to0xString());

            // Get the proof
            MMRProof proof = mmrnode->getMMR().getProofToPeak(input->getMMREntryNumber());

            // Create the CoinProof
            auto ccopy = input->deepCopy();
            auto pproof = std::make_shared<MMRProof>(proof);
            auto cp = std::make_unique<CoinProof>(std::move(ccopy), pproof);

            // Add it to the witness data
            witness.addCoinProof(std::move(cp));

            // Add the script proofs
            std::string scraddress = input->getAddress().to0xString();
            auto srow = walletdb.getScriptFromAddress(scraddress);
            if (!srow) {
                throw CommandException(std::string("SERIOUS ERROR script missing for simple address : ") + scraddress);
            }
            auto pscr = std::make_unique<ScriptProof>(srow->getScript());
            witness.addScript(std::move(pscr));

            // Add this address / public key to the list we need to sign as
            std::string pubkey = srow->getPublicKey();
            if (std::find(reqsigs.begin(), reqsigs.end(), pubkey) == reqsigs.end()) {
                reqsigs.push_back(pubkey);
            }
        }

        // Now add the output
        auto recipient = std::make_unique<Coin>(Coin::COINID_OUTPUT, sendaddress, colorminima, Token::TOKENID_CREATE, true);

        // Is there a Web Validation URL
        if (existsParam("webvalidate")) {
            // Add to the description
            jsonname->put("webvalidate", getParam("webvalidate"));
        }

        // Are we signing the token
        if (existsParam("signtoken")) {
            // What is the coinid of the first input
            MiniData firstcoinid = transaction.getAllInputs().at(0)->getCoinID();

            // Calculate the CoinID.. It's the first output
            MiniData tokencoinid = transaction.calculateCoinID(firstcoinid, 0);

            // Get the Public Key
            std::string sigpubkey = getParam("signtoken");

            // Now sign the coinid
            auto sig = walletdb.signData(sigpubkey, tokencoinid);

            // Get the MiniData version
            auto sigdata = MiniData::getMiniDataVersion(*sig);

            // Get the Pubkey.. add it to the JSON
            jsonname->put("signtype", std::string("minima"));
            jsonname->put("signedby", sigpubkey);
            jsonname->put("signature", sigdata->to0xString());
        }

        // Let's create the token
        auto createtoken = std::make_unique<Token>(
            Coin::COINID_OUTPUT,
            MiniNumber(scale),
            colorminima,
            MiniString(jsonname->toString()),
            MiniString(script));

        // Set the Create Token Details
        recipient->setToken(std::move(createtoken));

        // Add to the transaction
        transaction.addOutput(std::move(recipient));

        // Do we need to send change
        if (change.isMore(MiniNumber::ZERO())) {
            // Create a new address
            auto newwalletaddress = MinimaDB::getDB()->getWallet().getDefaultAddress();
            MiniData chgaddress(newwalletaddress->getAddress());

            auto changecoin = std::make_unique<Coin>(Coin::COINID_OUTPUT, chgaddress, change, Token::TOKENID_MINIMA, false);
            transaction.addOutput(std::move(changecoin));
        }

        // Are there any State Variables
        std::string state_json = state_obj->toJSONString();
        auto kv = parseSimpleObjectKV(state_json);
        for (const auto& p : kv) {
            // The port
            int port = 0;
            try {
                port = std::stoi(p.first);
            } catch (...) {
                continue; // skip invalid port
            }

            // Create a state variable
            auto sv = std::make_unique<StateVariable>(port, p.second);

            // Add to the transaction
            transaction.addStateVariable(std::move(sv));
        }

        // Compute the correct CoinID
        TxPoWGenerator::precomputeTransactionCoinID(transaction);

        // Calculate the TransactionID
        transaction.calculateTransactionID();

        // Now that we have constructed the transaction - lets sign it
        for (const auto& pubkey : reqsigs) {
            // Use the wallet
            auto signature = walletdb.signData(pubkey, transaction.getTransactionID());

            // Add it
            witness.addSignature(std::move(signature));
        }

        // The final TxPoW
        auto txpow_ptr = TxPoWGenerator::generateTxPoW(transaction, witness);

        // Calculate the size
        txpow_ptr->calculateTXPOWID();

        // Sync or Async mining
        if (minesync) {
            bool success = Main::getInstance()->getTxPoWMiner().MineMaxTxPoW(*txpow_ptr, 120000);

            if (!success) {
                throw CommandException("FAILED TO MINE txn in 120 seconds !?");
            }

        } else {
            Main::getInstance()->getTxPoWMiner().mineTxPoWAsync(*txpow_ptr);
        }

        // All good
        ret->put("response", txpow_ptr->toJSON());

        return ret;

    } catch (const CommandException& e) {
        ret->put("status", false);
        ret->put("pending", false);
        ret->put("error", std::string(e.what()));
        return ret;
    } catch (const std::exception& e) {
        ret->put("status", false);
        ret->put("pending", false);
        ret->put("error", std::string("Exception: ") + e.what());
        return ret;
    } catch (...) {
        ret->put("status", false);
        ret->put("pending", false);
        ret->put("error", "Unknown error occurred in tokencreate");
        return ret;
    }
}

Command* tokencreate::getFunction() {
    return new tokencreate();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org