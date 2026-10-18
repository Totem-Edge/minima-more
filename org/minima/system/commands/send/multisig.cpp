#include "org/minima/system/commands/send/multisig.hpp"

#include <chrono>
#include <sstream>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/backup/vault.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No specific Windows-only behavior required here.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::txndb::TxnDB;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::database::wallet::Wallet;
using org::minima::objects::Address;
using org::minima::objects::Coin;
using org::minima::objects::StateVariable;
using org::minima::objects::Transaction;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::utils::Crypto;
using org::minima::utils::MiniFile;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

static const JSONObject* anyToJSONObjectConst(const std::any& a) {
    if (a.type() == typeid(JSONObject)) {
        return &std::any_cast<const JSONObject&>(a);
    }
    if (a.type() == typeid(std::shared_ptr<JSONObject>)) {
        const auto& sp = std::any_cast<const std::shared_ptr<JSONObject>&>(a);
        return sp.get();
    }
    if (a.type() == typeid(std::unique_ptr<JSONObject>)) {
        const auto& up = std::any_cast<const std::unique_ptr<JSONObject>&>(a);
        return up.get();
    }
    return nullptr;
}

static bool anyToString(const std::any& a, std::string& out) {
    if (a.type() == typeid(std::string)) {
        out = std::any_cast<const std::string&>(a);
        return true;
    }
    if (a.type() == typeid(const char*)) {
        out = std::any_cast<const char*>(a);
        return true;
    }
    return false;
}

const std::string multisig::MULTISIG_CONTRACT =
    "LET root=PREVSTATE(1) IF root NEQ 0x21 THEN IF SIGNEDBY(root) THEN RETURN TRUE ENDIF ENDIF LET n=PREVSTATE(2) LET m=PREVSTATE(3) LET script=[RETURN MULTISIG(]+STRING(n) LET counter=0 WHILE counter LT m DO LET script=script+[ ]+STRING(PREVSTATE(counter+4)) LET counter=INC(counter) ENDWHILE LET script=script+[)] EXEC script";

multisig::multisig()
    : org::minima::system::commands::Command("multisig",
          "Create a multisig coin that can be used by root OR n of m txns") {}

std::string multisig::getFullHelp() const {
    return std::string("\nmultisig\n"
        "\n"
        "Create a multisig coin that can only be used in a txn signed by root OR n of m given public keys.\n"
        "\n"
        "Can provide the Vault password to temporarily decrypt the private keys when creating the coin or signing a transaction.\n"
        "\n"
        "action: \n"
        "    create : create a new multisig coin.\n"
        "    getkey : returns one of your default public keys to be provided when creating the coin.\n"
        "    list : lists all existing multisig coins.\n"
        "    spend : creates an unsigned transaction (.txn) file to spend a specified amount of a multisig coin.\n"
        "    sign : signs a multisig transaction (.txn) file with the relevant public keys and outputs a new signed file.\n"
        "    post : posts the transaction to the network, must be signed as required by the contract before posting.\n"
        "    view : view the details of a multisig transaction file.\n"
        "\n"
        "id: (optional)\n"
        "    Create a multisig coin with an id or list by id.\n"
        "    Should be unique. This cannot be retrieved later.\n"
        "    The id is hashed and stored as state variable 0.\n"
        "\n"
        "amount: (optional)\n"
        "    The amount to lock in the multisig coin or the amount to spend.\n"
        "\n"
        "publickeys: (optional)\n"
        "    The full list of public keys that can sign the multisig transaction, in the format [\"pubkey1\",..,\"pubkeym\"]\n"
        "\n"
        "root: (optional)\n"
        "    A root public key which can spend the coin without passing the required signature threshold.\n"
        "\n"
        "required: (optional)\n"
        "    The minimum number of public keys from the list required to sign a txn which spends from the multisig.\n"
        "\n"
        "coinid: (optional)\n"
        "    The coinid of the multisig coin to spend. Alternatively, use the id.\n"
        "\n"
        "address: (optional)\n"
        "    The address to send the specified amount to.\n"
        "\n"
        "file: (optional)\n"
        "    The transaction (.txn) file to create, sign or post.\n"
        "\n"
        "password: (optional)\n"
        "    Vault password to decrypt the private keys. Use with action:create and action:sign if the node is password locked.\n"
        "    Keys will be re-encypted after.\n"
        "\n"
        "Examples:\n"
        "\n"
        "multisig action:create id:2of3multisig amount:100 publickeys:[\"0xFED5..\",\"0xABD6..\",\"0xFD8B..\"] required:2 password:your_password\n"
        "\n"
        "multisig action:create id:3of3multisigroot amount:100 publickeys:[\"0xFED5..\",\"0xABD6..\",\"0xFD8B..\"] required:3 root:0xFFE..\n"
        "\n"
        "multisig action:list\n"
        "\n"
        "multisig action:list id:2of3multisig\n"
        "\n"
        "multisig action:spend id:3of3multisigroot amount:5 address:0xFF..\n"
        "\n"
        "multisig action:spend coinid:0x17EA.. amount:5 address:0xFF.. file:multisig.txn\n"
        "\n"
        "multisig action:sign file:multispend_1673351592845.txn\n"
        "\n"
        "multisig action:sign file:multisig.txn password:your_password\n"
        "\n"
        "multisig action:view file:multisig.txn\n"
        "\n"
        "multisig action:post file:signed_multispend_1673351592845.txn\n"
        "\n"
        "multisig action:post file:signed_multisig.txn\n");
}

std::vector<std::string> multisig::getValidParams() const {
    return std::vector<std::string>{
        "id","action","root","required","file","publickeys","amount",
        "tokenid","coinid","address","password","mine"
    };
}

std::unique_ptr<JSONObject> multisig::runCommand() {
    auto ret = getJSONReply();

    // What are we doing..
    std::string action = getParam("action");

    // The actual address (contract address)
    std::string msaddress = Address(multisig::MULTISIG_CONTRACT).getAddressData().to0xString();

    // ID of the custom transaction
    std::string randomid = MiniData::getRandomData(32).to0xString();

    if (action == "create") {
        // Optional ID
        std::string id;
        if (existsParam("id")) {
            MiniString sid(getParam("id"));
            id = Crypto::getInstance().hashObject(sid).to0xString();
        }

        // Optional root key
        std::string root = getParam("root", "0x21");
        if (root != "0x21") {
            if (!(root.rfind("0x", 0) == 0) || root.size() != 66) {
                throw org::minima::system::commands::CommandException("Invalid root key : " + root);
            }
        }

        // Required params
        auto amount = getNumberParam("amount");
        std::string tokenid = getParam("tokenid", "0x00");

        // Number of signatures required
        auto required = getNumberParam("required");

        // Get the pubkeys
        int totalkeys = 0;
        std::vector<std::string> allkeys;
        auto pubkeys = getJSONArrayParam("publickeys");
        for (std::size_t i = 0; i < pubkeys->size(); ++i) {
            std::string pubkey;
            if (!anyToString(pubkeys->at(i), pubkey)) {
                throw org::minima::system::commands::CommandException("Invalid public key value type");
            }

            if (!(pubkey.rfind("0x", 0) == 0) || pubkey.size() != 66) {
                throw org::minima::system::commands::CommandException("Invalid public key : " + pubkey);
            }

            allkeys.push_back(pubkey);
            totalkeys++;
        }

        if (totalkeys < required->getAsInt()) {
            throw org::minima::system::commands::CommandException("Cannot have LESS keys than required!");
        }

        // Construct the state params JSON string identically to Java
        std::string stateparams;
        if (id.empty()) {
            stateparams = std::string("{\"1\":\"") + root + "\",\"2\":\"" + required->toString() + "\",\"3\":\"" + std::to_string(totalkeys) + "\"";
        } else {
            stateparams = std::string("{\"0\":\"") + id + "\",\"1\":\"" + root + "\",\"2\":\"" + required->toString() + "\",\"3\":\"" + std::to_string(totalkeys) + "\"";
        }

        int counter = 4;
        for (const auto& pubk : allkeys) {
            stateparams += std::string(",\"") + std::to_string(counter) + "\":\"" + pubk + "\"";
            ++counter;
        }
        stateparams += "}";

        // Construct send function
        std::string sendfunction;
        if (existsParam("password")) {
            std::string password = getParam("password");
            sendfunction = "send password:" + password + " tokenid:" + tokenid + " amount:" + amount->toString() + " address:" + msaddress + " state:" + stateparams;
        } else {
            sendfunction = "send tokenid:" + tokenid + " amount:" + amount->toString() + " address:" + msaddress + " state:" + stateparams;
        }

        // Run it
        auto result = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(sendfunction);

        // First element should be a JSONObject
        const JSONObject* sendresult = nullptr;
        if (result && result->size() > 0) {
            sendresult = anyToJSONObjectConst(result->at(0));
        }

        if (sendresult && sendresult->getBoolean("status")) {
            JSONObject sender;
            // Copy response payload
            sender.put("send", sendresult->get("response"));
            sender.put("id", id);
            ret->put("response", sender);
        } else {
            ret->put("status", false);
            if (sendresult) {
                if (sendresult->containsKey("message")) {
                    ret->put("message", sendresult->get("message"));
                } else if (sendresult->containsKey("error")) {
                    ret->put("message", sendresult->get("error"));
                } else {
                    // Whole object
                    ret->put("message", *sendresult);
                }
            } else {
                // Could not parse result
                ret->put("message", std::string("Unknown send result"));
            }
        }

    } else if (action == "getkey") {
        // Get default address
        Wallet& wallet = MinimaDB::getDB()->getWallet();
        auto scrow = wallet.getDefaultAddress();
        std::string key = scrow ? scrow->getPublicKey() : std::string();

        JSONObject keyjson;
        keyjson.put("publickey", key);
        ret->put("response", keyjson);

    } else if (action == "list") {
        // Get the tree tip
        auto& tree = MinimaDB::getDB()->getTxPoWTree();
        auto tip = tree.getTip();

        // List all multisig coins (relevant, address filter = msaddress)
        std::vector<std::shared_ptr<Coin>> coins = TxPoWSearcher::searchCoins(
            tip, true,
            false, MiniData::ZERO_TXPOWID(),
            false, MiniNumber::ZERO(),
            true, MiniData(msaddress),
            false, MiniData::ZERO_TXPOWID(),
            false);

        if (existsParam("id")) {
            // Hash of the ID
            MiniString sid(getParam("id"));
            std::string id = Crypto::getInstance().hashObject(sid).to0xString();

            JSONArray coinarr;
            for (const auto& cc : coins) {
                if (!cc) continue;
                const auto& allstate = cc->getState();
                for (const auto& statevar : allstate) {
                    if (statevar && statevar->getData().toString() == id) {
                        coinarr.add(cc->toJSON());
                    }
                }
            }
            ret->put("response", coinarr);
        } else {
            JSONArray coinarr;
            for (const auto& cc : coins) {
                if (cc) {
                    coinarr.add(cc->toJSON());
                }
            }
            ret->put("response", coinarr);
        }

    } else if (action == "spend") {
        // Search via id or coinid
        std::string coinid;
        if (existsParam("id")) {
            MiniString sid(getParam("id"));
            std::string id = Crypto::getInstance().hashObject(sid).to0xString();

            // Get the tree tip
            auto& tree = MinimaDB::getDB()->getTxPoWTree();
            auto tip = tree.getTip();

            // List multisig coins
            std::vector<std::shared_ptr<Coin>> coins = TxPoWSearcher::searchCoins(
                tip, true,
                false, MiniData::ZERO_TXPOWID(),
                false, MiniNumber::ZERO(),
                true, MiniData(msaddress),
                false, MiniData::ZERO_TXPOWID(),
                false);

            std::shared_ptr<Coin> fcoin;
            for (const auto& cc : coins) {
                if (!cc) continue;
                const auto& allstate = cc->getState();
                bool found = false;
                for (const auto& statevar : allstate) {
                    if (statevar && statevar->getData().toString() == id) {
                        fcoin = cc;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }

            if (fcoin) {
                coinid = fcoin->getCoinID().to0xString();
            } else {
                throw org::minima::system::commands::CommandException("MultiSig with id not found : " + id);
            }
        } else {
            coinid = getParam("coinid");
        }

        // Find the coin
        std::shared_ptr<Coin> cc = TxPoWSearcher::searchCoin(MiniData(coinid), false);
        if (!cc) {
            throw org::minima::system::commands::CommandException("CoinID not found : " + coinid);
        }

        // Amount
        auto amount = getNumberParam("amount");

        // Change
        MiniNumber change = MiniNumber::ZERO();
        if (cc->getTokenID().isEqual(MiniData::ZERO_TXPOWID())) {
            change = cc->getAmount().sub(*amount);
        } else {
            // It's a token - use scaled token amount
            MiniNumber scaletoken = cc->getTokenAmount();
            change = scaletoken.sub(*amount);
        }

        // Destination and source address
        std::string tokenid = cc->getTokenID().to0xString();
        std::string address = getAddressParam("address");
        std::string coinaddress = cc->getAddress().to0xString();

        // Cannot send back to yourself (since we do not store state)
        if (address == coinaddress) {
            throw org::minima::system::commands::CommandException("CANNOT send funds back to yourself!");
        }

        // Transaction file name
        auto nowms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
        std::string txnname_default = "multispend_" + std::to_string(nowms) + ".txn";
        std::string txnname = getParam("file", txnname_default);

        // Build txn sender command
        std::ostringstream txnsender;
        txnsender
            << "txncreate id:" << randomid << ";"
            << "txninput  id:" << randomid << " coinid:" << coinid << ";"
            << "txnoutput id:" << randomid << " storestate:false amount:" << amount->toString()
            << " address:" << address << " tokenid:" << tokenid << ";";

        if (change.isMore(MiniNumber::ZERO())) {
            // Copy the complete coin state
            const auto& allstate = cc->getState();
            for (const auto& statevar : allstate) {
                if (!statevar) continue;
                txnsender << "txnstate id:" << randomid
                          << " port:" << statevar->getPort()
                          << " value:" << statevar->getData().toString() << ";";
            }
            // And the change
            txnsender << "txnoutput id:" << randomid << " storestate:true amount:" << change.toString()
                      << " address:" << coinaddress << " tokenid:" << tokenid << ";";
        }

        // Finish
        txnsender << "txnexport id:" << randomid << " file:" << txnname << ";"
                  << "txndelete id:" << randomid;

        auto result = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(txnsender.str());
        ret->put("response", *result);

    } else if (action == "sign") {
        // Which file
        std::string file = getParam("file");

        // Actual file and signed file
        std::filesystem::path actualfile = MiniFile::createBaseFile(file);
        std::filesystem::path signedtxn = actualfile.parent_path() / ("signed_" + actualfile.filename().string());

        // Import signer
        std::string txnsigner = "txnimport id:" + randomid + " file:" + file + ";";
        auto result = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(txnsigner);
        const JSONObject* importres = nullptr;
        if (result && result->size() > 0) {
            importres = anyToJSONObjectConst(result->at(0));
        }
        if (!importres || !importres->getBoolean("status")) {
            std::string err = importres ? importres->getString("error", "Unknown import error") : "Unknown import error";
            throw org::minima::system::commands::CommandException(err);
        }

        // Reset signer commands
        txnsigner.clear();

        // Wallet
        Wallet& wallet = MinimaDB::getDB()->getWallet();

        // Find public keys in the input coin's state
        TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();
        TxnRow* txnrow = db.getTransactionRow(randomid);
        if (!txnrow) {
            throw org::minima::system::commands::CommandException("Transaction not found in TxnDB : " + randomid);
        }
        Transaction& trans = txnrow->getTransaction();
        auto& inputs = trans.getAllInputs();
        if (inputs.empty() || !inputs[0]) {
            throw org::minima::system::commands::CommandException("Invalid transaction inputs");
        }
        Coin& multicoin = *inputs[0];
        const auto& allstate = multicoin.getState();
        for (const auto& statevar : allstate) {
            if (!statevar) continue;
            std::string possiblepubkey = statevar->getData().toString();
            auto key = wallet.getKeyFromPublic(possiblepubkey);
            if (key) {
                txnsigner += "txnsign id:" + randomid + " publickey:" + possiblepubkey + ";";
            }
        }

        // Finish export and delete (fixed concatenation)
        txnsigner += "txnexport id:" + randomid + " file:" + signedtxn.string() + ";";
        txnsigner += "txndelete id:" + randomid;

        // Unlock DB at the start - rather than every time you run txnsign
        bool passwordlock = false;
        if (existsParam("password") && !MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
            org::minima::system::commands::backup::vault::passowrdUnlockDB(getParam("password"));
            passwordlock = true;
        }

        // Run it
        result = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(txnsigner);

        // Relock DB if needed
        if (passwordlock) {
            org::minima::system::commands::backup::vault::passwordLockDB(getParam("password"));
        }

        ret->put("response", *result);

    } else if (action == "post") {
        std::string file = getParam("file");
        std::ostringstream txnsigner;
        txnsigner << "txnimport id:" << randomid << " file:" << file << ";"
                  << "txnpost   id:" << randomid << " mine:true auto:true;"
                  << "txndelete id:" << randomid;

        auto result = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(txnsigner.str());
        ret->put("response", *result);

    } else if (action == "view") {
        std::string file = getParam("file");
        std::ostringstream txnview;
        txnview << "txnimport id:" << randomid << " file:" << file << ";"
                << "txndelete id:" << randomid;

        auto result = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(txnview.str());
        ret->put("response", *result);

    } else {
        throw org::minima::system::commands::CommandException("Invalid action : " + action);
    }

    return ret;
}

std::filesystem::path multisig::getRequiredFile() {
    // What is the filename - could be relative or absolute
    std::string filename = getParam("file");
    // Convert to an actual file
    std::filesystem::path theactualfile = MiniFile::createBaseFile(filename);
    return theactualfile;
}

org::minima::system::commands::Command* multisig::getFunction() {
    return new multisig();
}

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org