#include "org/minima/system/commands/send/multisigread.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <sstream>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"

#include "org/minima/database/minima_d_b.hpp"
// TxPoWTree header (ensure this path matches your project)
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/wallet/script_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

#include "org/minima/system/brains/tx_po_w_searcher.hpp"

#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

using org::minima::database::MinimaDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::wallet::Wallet;
// using org::minima::database::wallet::ScriptRow;
using org::minima::objects::Address;
using org::minima::objects::Coin;
using org::minima::objects::StateVariable;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::utils::Crypto;
using org::minima::utils::MiniFile;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

const std::string multisigread::MULTISIG_CONTRACT =
    "LET root=PREVSTATE(1) IF root NEQ 0x21 THEN IF SIGNEDBY(root) THEN RETURN TRUE ENDIF ENDIF LET n=PREVSTATE(2) LET m=PREVSTATE(3) LET script=[RETURN MULTISIG(]+STRING(n) LET counter=0 WHILE counter LT m DO LET script=script+[ ]+STRING(PREVSTATE(counter+4)) LET counter=INC(counter) ENDWHILE LET script=script+[)] EXEC script";

multisigread::multisigread()
    : Command("multisigread",
              "View, list or start a spend of a multisig coin that can be used by root OR n of m txns") {
}

std::string multisigread::getFullHelp() const {
    // ... (rest of help string) ...
    return std::string("\nmultisigread\n"
                       "\n"
                       "Utility Function for multisig in READ mode\n"
                       "\n"
                       "List, create a spend or get a key for a nultisig coin that can only be used in a txn signed by root OR n of m given public keys.\n"
                       "\n"
                       "action: \n"
                       "    getkey : returns one of your default public keys to be provided when creating the coin.\n"
                       "    list : lists all existing multisig coins.\n"
                       "    spend : creates an unsigned transaction (.txn) file to spend a specified amount of a multisig coin.\n"
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
                       "coinid: (optional)\n"
                       "    The coinid of the multisig coin to spend. Alternatively, use the id.\n"
                       "\n"
                       "address: (optional)\n"
                       "    The address to send the specified amount to.\n"
                       "\n"
                       "file: (optional)\n"
                       "    The transaction (.txn) file to view or post.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "multisig action:getkey\n"
                       "\n"
                       "multisig action:list\n"
                       "\n"
                       "multisig action:list id:2of3multisig\n"
                       "\n"
                       "multisig action:spend id:3of3multisigroot amount:5 address:0xFF..\n"
                       "\n"
                       "multisig action:spend coinid:0x17EA.. amount:5 address:0xFF.. file:multisig.txn\n"
                       "\n"
                       "multisig action:view file:multisig.txn\n"
                       "\n"
                       "multisig action:post file:signed_multispend_1673351592845.txn\n"
                       "\n"
                       "multisig action:post file:signed_multisig.txn\n");
}

std::vector<std::string> multisigread::getValidParams() const {
    return std::vector<std::string>{
        "id",       "action",   "root",    "required", "file",   "publickeys",
        "amount",   "tokenid",  "coinid",  "address",  "password"};
}

std::unique_ptr<JSONObject> multisigread::runCommand() {
    auto ret = getJSONReply();

    // What are we doing..
    std::string action = getParam("action");

    // The actual address
    Address msaddr(multisigread::MULTISIG_CONTRACT);
    std::string msaddress = msaddr.getAddressData().to0xString();

    // ID of the custom transaction
    std::string randomid = MiniData::getRandomData(32).to0xString();

    if (action == "getkey") {
        //
        // Get wallet as a reference
        //
        Wallet& wallet = MinimaDB::getDB()->getWallet();
        
        //
        // FIX: Removed the 'if (!wallet)' check as 'wallet' is now a reference
        //

        // Get default address using dot . operator
        //
        ScriptRow scrow = *wallet.getDefaultAddress();

        // Get the public key
        std::string key = scrow.getPublicKey();

        JSONObject keyjson;
        keyjson.put("publickey", key);

        ret->put("response", keyjson);

    } else if (action == "list") {
        //
        // Get tip using dot . operator
        //
        auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

        // List all the multi sig coins you have..
        auto coins = TxPoWSearcher::searchCoins(
            tip, true,                                   // relevant
            false, MiniData::ZERO_TXPOWID(),               // check coinid
            false, MiniNumber::ZERO(),                     // check amount
            true, MiniData(msaddress),                   // check address
            false, MiniData::ZERO_TXPOWID(),               // check tokenid
            false                                        // simple only
        );

        // Are we searching via id
        if (existsParam("id")) {
            // Get the hash of the ID
            MiniString sid(getParam("id"));
            std::string id = Crypto::getInstance().hashObject(sid).to0xString();

            // Search for it..
            JSONArray coinarr;
            for (const auto& cc : coins) {
                const auto& allstate = cc->getState();
                for (const auto& statevar : allstate) {
                    if (statevar->getData().toString() == id) {
                        coinarr.add(cc->toJSON());
                    }
                }
            }
            ret->put("response", coinarr);
        } else {
            // Put it all in an array
            JSONArray coinarr;
            for (const auto& cc : coins) {
                coinarr.add(cc->toJSON());
            }
            ret->put("response", coinarr);
        }

    } else if (action == "spend") {
        // Are we searching via id or coinid
        std::string coinid;

        if (existsParam("id")) {
            // Get the hash of the ID
            MiniString sid(getParam("id"));
            std::string id = Crypto::getInstance().hashObject(sid).to0xString();

            //
            // Get tip using dot . operator
            //
            auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

            // List all the multi sig coins you have..
            auto coins = TxPoWSearcher::searchCoins(
                tip, true,                                   // relevant
                false, MiniData::ZERO_TXPOWID(),               // check coinid
                false, MiniNumber::ZERO(),                     // check amount
                true, MiniData(msaddress),                   // check address
                false, MiniData::ZERO_TXPOWID(),               // check tokenid
                false                                        // simple only
            );

            // Search for it..
            std::shared_ptr<Coin> fcoin = nullptr;
            for (const auto& cc : coins) {
                const auto& allstate = cc->getState();
                for (const auto& statevar : allstate) {
                    if (statevar->getData().toString() == id) {
                        fcoin = cc;
                        break;
                    }
                }
                if (fcoin) {
                    break;
                }
            }

            if (fcoin) {
                coinid = fcoin->getCoinID().to0xString();
            } else {
                throw CommandException(std::string("MultiSig with id not found : ") + id);
            }
        } else {
            // Which coin
            coinid = getParam("coinid");
        }

        // Find the coin
        auto cc = TxPoWSearcher::searchCoin(MiniData(coinid), false);
        if (!cc) {
            throw CommandException(std::string("CoinID not found : ") + coinid);
        }

        // How much
        auto amountPtr = getNumberParam("amount");
        MiniNumber amount = *amountPtr;

        // Any change
        MiniNumber change = MiniNumber::ZERO();
        if (cc->getTokenID().isEqual(MiniData::ZERO_TXPOWID())) {
            change = cc->getAmount().sub(amount);
        } else {
            // It's a token - use scaled token amount
            MiniNumber scaletoken = cc->getTokenAmount();
            change = scaletoken.sub(amount);
        }

        // Which key do we sign with
        std::string tokenid = cc->getTokenID().to0xString();
        std::string address = getParam("address");
        std::string coinaddress = cc->getAddress().to0xString();

        // CANNOT send funds back to yourself as do not store the state
        if (address == coinaddress) {
            throw CommandException("CANNOT send funds back to yourself!");
        }

        // The txnname..
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now());
        long long millis = now.time_since_epoch().count();
        std::string txnname_default = std::string("multispend_") + std::to_string(millis) + ".txn";
        std::string txnname = getParam("file", txnname_default);

        // Create a txn..
        std::string txnsender =
            std::string("txncreate id:") + randomid + ";" +
            "txninput  id:" + randomid + " coinid:" + coinid + ";" +
            "txnoutput id:" + randomid + " storestate:false amount:" + amount.toString() +
            " address:" + address + " tokenid:" + tokenid + ";";

        // Is there change
        if (change.isMore(MiniNumber::ZERO())) {
            // Copy the complete coin state
            const auto& allstate = cc->getState();
            for (const auto& statevar : allstate) {
                //
                // (Previous Fix 5)
                //
                txnsender += "txnstate id:" + randomid + " port:" + std::to_string(statevar->getPort()) +
                             " value:" + statevar->getData().toString() + ";";
            }

            // And the change
            //
            // (Previous Fix 6)
            //
            txnsender += "txnoutput id:" + randomid + " storestate:true amount:" + change.toString() +
                         " address:" + coinaddress + " tokenid:" + tokenid + ";";
        }

        // And finish off..
        txnsender += std::string("txnexport id:") + randomid + " file:" + txnname + ";" +
                     "txndelete id:" + randomid;

        // Run it..
        auto resultPtr = CommandRunner::getRunner()->runMultiCommand(txnsender);
        JSONArray responseArr;
        if (resultPtr) {
            responseArr = *resultPtr;
        }
        ret->put("response", responseArr);

    } else if (action == "post") {
        // Which file..
        std::string file = getParam("file");
        std::string txnsigner =
            std::string("txnimport id:") + randomid + " file:" + file + ";" +
            "txnpost   id:" + randomid + " mine:true auto:true;" +
            "txndelete id:" + randomid;

        // Run it..
        auto resultPtr = CommandRunner::getRunner()->runMultiCommand(txnsigner);
        JSONArray responseArr;
        if (resultPtr) {
            responseArr = *resultPtr;
        }
        ret->put("response", responseArr);

    } else if (action == "view") {
        std::string file = getParam("file");
        //
        // (Previous Fix 7)
        //
        std::string txnview =
            std::string("txnimport id:") + randomid + " file:" + file + ";" +
            "txndelete id:" + randomid;

        //
        // (Previous Fix 8)
        //
        auto resultPtr = CommandRunner::getRunner()->runMultiCommand(txnview);
        JSONArray responseArr;
        if (resultPtr) {
            responseArr = *resultPtr;
        }
        ret->put("response", responseArr);

    } else {
        throw CommandException(std::string("Invalid action : ") + action);
    }

    return ret;
}

std::filesystem::path multisigread::getRequiredFile() {
    // What is the filename - could be relative or absolute
    std::string filename = getParam("file");

    // Convert to an actual file path
    std::filesystem::path theactualfile = MiniFile::createBaseFile(filename);

    return theactualfile;
}

Command* multisigread::getFunction() {
    return new multisigread();
}

}
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
