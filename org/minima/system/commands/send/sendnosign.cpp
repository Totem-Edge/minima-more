#include "org/minima/system/commands/send/sendnosign.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "org/minima/system/commands/command_exception.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/txn/txnutils.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::wallet::Wallet;

using org::minima::objects::Address;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::Transaction;
using org::minima::objects::TxPoW;
using org::minima::objects::Witness;
using org::minima::objects::StateVariable;

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;

using org::minima::system::Main;
using org::minima::system::brains::TxPoWGenerator;
using org::minima::system::brains::TxPoWMiner;
using org::minima::system::brains::TxPoWSearcher;

using org::minima::system::commands::txn::txnutils;
using org::minima::system::params::GlobalParams;

using org::minima::utils::MiniFile;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;


namespace {

static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return out;
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin());
}

static std::unique_ptr<Token> cloneToken(const Token& tkn) {
    const MiniData& coinid = tkn.getCoinID();
    const MiniNumber& scale = tkn.getScale();
    const MiniNumber& miniamt = tkn.getAmount();
    const MiniString& name = tkn.getName();
    const MiniString& script = tkn.getTokenScript();
    const MiniNumber& created = tkn.getCreated();
    return std::make_unique<Token>(coinid, scale, miniamt, name, script, created);
}

// Parse flat JSONObject like {"0":"0x..","1":"..."} into vector of (port,value).
static std::vector<std::pair<int, std::string>>
parse_flat_state_object(const std::string& json) {
    std::vector<std::pair<int, std::string>> res;
    auto trim = [](const std::string& s)->std::string {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    };

    std::string s = trim(json);
    if (s.empty() || s.front() != '{' || s.back() != '}') return res;
    s = s.substr(1, s.size()-2);

    std::vector<std::string> pairs;
    std::string cur;
    bool inq = false;
    for (char c : s) {
        if (c == '"') {
            inq = !inq;
            cur.push_back(c);
        } else if (c == ',' && !inq) {
            pairs.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) pairs.push_back(trim(cur));

    for (const auto& p : pairs) {
        bool inq2 = false;
        size_t pos = std::string::npos;
        for (size_t i = 0; i < p.size(); ++i) {
            char c = p[i];
            if (c == '"') inq2 = !inq2;
            if (c == ':' && !inq2) { pos = i; break; }
        }
        if (pos == std::string::npos) continue;
        auto trim2 = trim;
        std::string k = trim2(p.substr(0, pos));
        std::string v = trim2(p.substr(pos+1));

        auto unquote = [](const std::string& x)->std::string {
            if (x.size() >= 2 && x.front()=='"' && x.back()=='"') {
                return x.substr(1, x.size()-2);
            }
            return x;
        };
        k = unquote(k);
        v = unquote(v);

        try {
            int port = std::stoi(k);
            res.emplace_back(port, v);
        } catch (...) {
            // ignore invalid port keys
        }
    }
    return res;
}

} // anonymous namespace

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

sendnosign::sendnosign()
: Command(
    "sendnosign",
    "(address:Mx..|0x..) (amount:) (multi:[address:amount,..]) (tokenid:) (state:{}) (burn:) (split:) (file:) (debug:) - Create a txn but don't sign it"
) {}

sendnosign::~sendnosign() = default;

std::vector<std::string> sendnosign::getValidParams() const {
    return std::vector<std::string>{
        "address","amount","multi","tokenid","state","burn","split","debug","dryrun","file","coinage"
    };
}

std::string sendnosign::getFullHelp() const {
    return
        "\nsendnosign\n"
        "\n"
        "Create a txn but don't sign it.\n"
        "\n"
        "Constructs and outputs an unsigned transaction to a file in the base folder\n"
        "\n"
        "The output .txn file can then be imported to an offline node for signing.\n"
        "\n"
        "Must be done from an online node as the MMR proofs for the input coins are added.\n"
        "\n"
        "Useful when the keys on an online node are wiped or password locked.\n"
        "\n"
        "address: (optional)\n"
        "    A Minima 0x or Mx wallet address or custom script address. Must also specify amount.\n"
        "\n"
        "amount: (optional)\n"
        "    The amount of Minima or custom tokens to send to the specified address.\n"
        "\n"
        "multi: (optional)\n"
        "    JSON Array listing addresses and amounts to send in one transaction.\n"
        "    Takes the format [address:amount,address2:amount2,..], with each set in double quotes.\n"
        "\n"
        "tokenid: (optional)\n"
        "    If sending a custom token, you must specify its tokenid. Defaults to Minima (0x00).\n"
        "\n"
        "state: (optional)\n"
        "    List of state variables, if sending coins to a script. A JSON object in the format {\"port\":\"value\",..}\n"
        "\n"
        "burn: (optional)\n"
        "    The amount of Minima to burn with this transaction.\n"
        "\n"
        "split: (optional)\n"
        "    Set the number of coins the recipient will receive, between 1 and 20. Default is 1.\n"
        "    The amount being sent will be split into multiple coins of equal value.\n"
        "    You can split your own coins by sending to your own address.\n"
        "    Useful if you want to send multiple transactions without waiting for change to be confirmed.\n"
        "\n"
        "file: (optional)\n"
        "    Specify the file to output otherwise default chosen\n"
        "\n"
        "debug: (optional)\n"
        "    true or false, true will print more detailed logs.\n"
        "\n"
        "Examples:\n"
        "\n"
        "sendnosign address:0xFF.. amount:10\n"
        "\n"
        "sendnosign address:0xFF.. amount:10 tokenid:0xFED5.. burn:0.1\n"
        "\n"
        "sendnosign address:0xFF.. amount:10 split:5 burn:0.1\n"
        "\n"
        "sendnosign multi:[\"0xFF..:10\",\"0xEE..:10\",\"0xDD..:10\"] split:20\n"
        "\n"
        "sendnosign amount:1 address:0xFF.. state:{\"0\":\"0xEE..\",\"1\":\"0xDD..\"}\n"
        "\n";
}

std::vector<std::shared_ptr<Coin>>
sendnosign::selectCoins(const std::vector<std::shared_ptr<Coin>>& coins,
                        const MiniNumber& /*target*/,
                        bool /*debug*/) {
    // Ambiguity note: Java calls send.selectCoins(coins, target, debug) but implementation is not provided.
    // To avoid inventing logic, we preserve input order.
    return coins;
}

std::unique_ptr<JSONObject> sendnosign::runCommand() {
    auto ret = getJSONReply();

    try {
        // Recipients
        std::vector<std::pair<MiniData, MiniNumber>> recipients;
        MiniNumber totalamount = MiniNumber::ZERO();

        if (existsParam("multi")) {
            std::unique_ptr<JSONArray> allrecips = getJSONArrayParam("multi");
            for (const auto& anyv : allrecips->elements()) {
                const std::string sendto = std::any_cast<std::string>(anyv);
                auto pos = sendto.find(':');
                if (pos == std::string::npos) {
                    throw org::minima::system::commands::CommandException("Invalid multi entry: " + sendto);
                }
                std::string address = sendto.substr(0, pos);
                std::string amountstr = sendto.substr(pos + 1);

                MiniData addr;
                std::string al = to_lower(address);
                if (starts_with(al, "mx")) {
                    try {
                        addr = Address::convertMinimaAddress(address);
                    } catch (const std::exception& exc) {
                        throw org::minima::system::commands::CommandException(exc.what());
                    }
                } else {
                    addr = MiniData(address);
                }

                MiniNumber amount(amountstr);
                totalamount = totalamount.add(amount);

                recipients.emplace_back(addr, amount);
            }
        } else {
            // Single recipient
            MiniData sendaddress(getAddressParam("address"));
            std::unique_ptr<MiniNumber> sendamount = getNumberParam("amount");
            totalamount = *sendamount;
            recipients.emplace_back(sendaddress, *sendamount);
        }

        // TokenID and debug
        std::string tokenid = getParam("tokenid", "0x00");
        bool debug = getBooleanParam("debug", false);

        // Burn
        std::unique_ptr<MiniNumber> burn_ptr = getNumberParam("burn", MiniNumber::ZERO());
        MiniNumber burn = *burn_ptr;
        if (burn.isLess(MiniNumber::ZERO())) {
            throw org::minima::system::commands::CommandException(
                std::string("Cannot have negative burn ") + burn.toString());
        }

        // Split
        std::unique_ptr<MiniNumber> split_ptr = getNumberParam("split", MiniNumber::ONE());
        MiniNumber split = *split_ptr;
        if (split.isLess(MiniNumber::ONE()) || split.isMore(MiniNumber::TWENTY())) {
            throw org::minima::system::commands::CommandException("Split outputs from 1 to 20");
        }

        // If sending Minima, include burn in total
        if (tokenid == "0x00") {
            totalamount = totalamount.add(burn);
        }

        // State
        std::unique_ptr<JSONObject> state_obj = std::make_unique<JSONObject>();
        if (existsParam("state")) {
            state_obj = getJSONObjectParam("state");
        }

        // Get tip and move up confirmation depth
        auto& tree = MinimaDB::getDB()->getTxPoWTree();
        std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

        int confdepth = GlobalParams::MINIMA_CONFIRM_DEPTH.getAsInt();
        for (int i = 0; i < confdepth; ++i) {
            tip = tip ? tip->getParent() : nullptr;
            if (!tip) {
                ret->put("status", false);
                ret->put("message", std::string("Insufficient blocks.."));
                return ret;
            }
        }

        // Access DBs and miner
        TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
        TxPoWMiner* txminer = &Main::getInstance()->getTxPoWMiner();

        // Coin age filter (optional param)
        std::unique_ptr<MiniNumber> coinage_ptr = getNumberParam("coinage", MiniNumber::ZERO());
        MiniNumber coinage = *coinage_ptr;

        // Find relevant unspent coins (simple only)
        std::vector<std::shared_ptr<Coin>> foundcoins =
            TxPoWSearcher::getRelevantUnspentCoins(tip, tokenid, true);

        std::vector<std::shared_ptr<Coin>> relcoins;
        MiniNumber mincoinblock = tip->getBlockNumber().sub(coinage);
        for (auto& c : foundcoins) {
            if (c->getBlockCreated().isLessEqual(mincoinblock)) {
                relcoins.push_back(c);
            }
        }

        if (relcoins.empty()) {
            throw org::minima::system::commands::CommandException(
                std::string("No Coins of tokenid:") + tokenid + " available!");
        }

        // Determine findamount for token scaling
        MiniNumber findamount = totalamount;
        Token* token_ptr = nullptr;
        if (tokenid != "0x00") {
            token_ptr = relcoins[0]->getToken();
            if (!token_ptr) {
                throw org::minima::system::commands::CommandException("Token metadata missing on coin");
            }
            auto scaledMinima = token_ptr->getScaledMinimaAmount(totalamount);
            findamount = *scaledMinima;
        }

        // Select coins (no-op selection due to missing send.selectCoins implementation)
        relcoins = selectCoins(relcoins, findamount, debug);

        // Iterate coins and collect until enough
        MiniNumber currentamount = MiniNumber::ZERO();
        std::vector<std::shared_ptr<Coin>> currentcoins;

        if (debug) {
            MinimaLogger::log("Coins that will be checked for transaction");
            for (auto& coin : relcoins) {
                MinimaLogger::log("Coin : " + coin->getAmount().toString() + " " + coin->getCoinID().to0xString());
            }
        }

        for (auto& coin : relcoins) {
            // Skip if being mined
            if (txminer && txminer->checkForMiningCoin(coin->getCoinID().to0xString())) {
                if (debug) {
                    MinimaLogger::log("Coin being mined : " + coin->getAmount().toString()
                                      + " " + coin->getCoinID().to0xString());
                }
                continue;
            }
            // Skip if in mempool
            if (txpdb.checkMempoolCoins(coin->getCoinID())) {
                if (debug) {
                    MinimaLogger::log("Coin in mempool : " + coin->getAmount().toString()
                                      + " " + coin->getCoinID().to0xString());
                }
                continue;
            }

            currentcoins.push_back(coin);

            if (tokenid == "0x00") {
                currentamount = currentamount.add(coin->getAmount());
            } else {
                if (!token_ptr) token_ptr = coin->getToken();
                auto amtToken = coin->getToken()->getScaledTokenAmount(coin->getAmount());
                currentamount = currentamount.add(*amtToken);
            }

            if (debug) {
                MinimaLogger::log("Coin added : " + coin->getAmount().toString()
                                  + " " + coin->getCoinID().to0xString()
                                  + " total:" + currentamount.toString());
            }

            if (currentamount.isMoreEqual(totalamount)) {
                break;
            }
        }

        // Token script must be simple
        if (token_ptr != nullptr) {
            std::string script = token_ptr->getTokenScript().toString();
            if (script != "RETURN TRUE") {
                ret->put("status", false);
                ret->put("message", std::string("Token script is not simple : ") + script);
                return ret;
            }
        }

        if (currentamount.isLess(totalamount)) {
            ret->put("status", false);
            ret->put("message",
                     std::string("Insufficient funds.. you only have ")
                        + currentamount.toString() + " require:" + totalamount.toString());
            return ret;
        }

        if (debug) {
            MinimaLogger::log("Total Coins used : " + std::to_string(currentcoins.size()));
        }

        // Change
        MiniNumber change = currentamount.sub(totalamount);

        // Construct txn
        Transaction transaction;
        Witness witness;

        // Build inputs (deep copy into transaction), track added coin IDs (for burn), and gather required sigs
        std::vector<std::string> addedcoinid;
        std::unordered_set<std::string> reqsigset;
        Wallet& walletdb = MinimaDB::getDB()->getWallet();

        for (auto& incoin : currentcoins) {
            // Add input
            std::unique_ptr<Coin> dc = incoin->deepCopy();
            transaction.addInput(std::move(dc));

            // Burn exclusion list
            addedcoinid.emplace_back(incoin->getCoinID().to0xString());

            // Required signatures (from wallet script rows)
            std::string scraddress = incoin->getAddress().to0xString();
            std::unique_ptr<ScriptRow> srow =
                walletdb.getScriptFromAddress(scraddress);
            if (!srow) {
                throw org::minima::system::commands::CommandException(
                    "SERIOUS ERROR script missing for simple address : " + scraddress);
            }
            std::string pubkey = srow->getPublicKey();
            reqsigset.insert(pubkey);
        }

        // Attach MMR and Script proofs for inputs
        txnutils::setMMRandScripts(transaction, witness);

        // Convert totalamount for token precisions if needed
        if (tokenid != "0x00") {
            if (!token_ptr) {
                throw org::minima::system::commands::CommandException("Token metadata missing");
            }
            std::unique_ptr<MiniNumber> tokenamount = token_ptr->getScaledMinimaAmount(totalamount);
            std::unique_ptr<MiniNumber> prectest = token_ptr->getScaledTokenAmount(*tokenamount);
            if (!prectest->isEqual(totalamount)) {
                throw org::minima::system::commands::CommandException(
                    std::string("Invalid Token amount to send.. ") + totalamount.toString());
            }
            totalamount = *tokenamount;
        } else {
            if (!totalamount.isValidMinimaValue()) {
                throw org::minima::system::commands::CommandException(
                    std::string("Invalid Minima amount to send.. ") + totalamount.toString());
            }
        }

        // Outputs
        int isplit = split.getAsInt();

        for (const auto& user : recipients) {
            // splitamount = user.amount / split
            MiniNumber splitamount = user.second.div(split);
            MiniData address = user.first;

            if (tokenid != "0x00") {
                // Use token scaling to Minima amount
                auto tmp = token_ptr->getScaledMinimaAmount(splitamount);
                splitamount = *tmp;
            }

            for (int i = 0; i < isplit; ++i) {
                // Create output
                auto recipient = std::make_unique<Coin>(Coin::COINID_OUTPUT, address, splitamount,
                                                        Token::TOKENID_MINIMA, true);

                if (tokenid != "0x00") {
                    recipient->resetTokenID(MiniData(tokenid));
                    recipient->setToken(cloneToken(*token_ptr));
                }

                transaction.addOutput(std::move(recipient));
            }
        }

        // Change output
        if (debug) {
            MinimaLogger::log("Change amount : " + change.toString());
        }

        if (change.isMore(MiniNumber::ZERO())) {
            std::unique_ptr<ScriptRow> newwalletaddress =
                MinimaDB::getDB()->getWallet().getDefaultAddress();
            MiniData chgaddress(newwalletaddress->getAddress());

            MiniNumber changeamount = change;
            if (tokenid != "0x00") {
                auto tmp = token_ptr->getScaledMinimaAmount(change);
                changeamount = *tmp;
            }

            auto changecoin = std::make_unique<Coin>(Coin::COINID_OUTPUT, chgaddress, changeamount,
                                                     Token::TOKENID_MINIMA, false);
            if (tokenid != "0x00") {
                changecoin->resetTokenID(MiniData(tokenid));
                changecoin->setToken(cloneToken(*token_ptr));
            }

            transaction.addOutput(std::move(changecoin));
        }

        // State variables (from JSON string)
        if (existsParam("state")) {
            std::string statejson = state_obj->toJSONString();
            auto statepairs = parse_flat_state_object(statejson);
            for (const auto& kv : statepairs) {
                int port = kv.first;
                const std::string& var = kv.second;
                auto sv = std::make_unique<StateVariable>(port, var);
                transaction.addStateVariable(std::move(sv));
            }
        }

        // Precompute CoinIDs and set TransactionID
        TxPoWGenerator::precomputeTransactionCoinID(transaction);
        transaction.calculateTransactionID();

        // Debug: required signatures
        if (debug) {
            MinimaLogger::log("Total signatures required : " + std::to_string(reqsigset.size()));
        }

        // Build TxPoW (with optional burn transaction)
        std::unique_ptr<org::minima::objects::TxPoW> txpow_ptr;

        if (tokenid != "0x00" && burn.isMore(MiniNumber::ZERO())) {
            // Create Burn Transaction - but NO signatures
            org::minima::database::userprefs::txndb::TxnRow burntxn =
                txnutils::createBurnTransaction(addedcoinid, transaction.getTransactionID(), burn, false);

            txpow_ptr = TxPoWGenerator::generateTxPoW(
                transaction, witness,
                &burntxn.getTransaction(), &burntxn.getWitness()
            );
        } else {
            txpow_ptr = TxPoWGenerator::generateTxPoW(transaction, witness);
        }

        // Compute txpow id/size
        txpow_ptr->calculateTXPOWID();

        // Choose output file
        std::filesystem::path txnfile;
        if (existsParam("file")) {
            txnfile = MiniFile::createBaseFile(getParam("file"));
        } else {
            auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now())
                           .time_since_epoch()
                           .count();
            std::ostringstream oss;
            oss << "unsignedtransaction-" << now << ".txn";
            txnfile = MiniFile::createBaseFile(oss.str());
        }

        // Write the TxPoW to file
        MiniFile::writeObjectToFile(txnfile, *txpow_ptr);

        // Response
        JSONObject resp;
        resp.put("txpow", txnfile.string());

        ret->put("response", resp);
        return ret;
    } catch (const org::minima::system::commands::CommandException&) {
        throw;
    } catch (const std::exception& ex) {
        throw org::minima::system::commands::CommandException(ex.what());
    }
}

org::minima::system::commands::Command* sendnosign::getFunction() {
    return new sendnosign();
}

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org