#include "org/minima/system/commands/search/keys.hpp"

#include <utility>
#include <cstdint>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/key_row.hpp" 
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/keys/tree_key.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/utils/b_i_p39.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

using org::minima::database::MinimaDB;
using org::minima::database::wallet::Wallet;
// Use the KeyRow from its proper namespace
using org::minima::database::wallet::KeyRow; 
using org::minima::objects::Address;
using org::minima::objects::base::MiniData;
using org::minima::objects::keys::TreeKey;
using org::minima::utils::BIP39;
using org::minima::utils::Crypto;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

namespace {

// THIS IS THE DUPLICATE FUNCTION - REMOVED
// static JSONObject makeKeyJSON(const KeyRow& kr) { ... }

} // anonymous namespace

keys::keys()
    : org::minima::system::commands::Command(
          "keys",
          "(action:list|new|checkkeys|genkey) (publickey:) - Get a list of all your public keys or create a new key") {
}

std::string keys::getFullHelp() const {
    // ... (content is fine) ...
    return "\nkeys\n"
           "\n"
           "Get a list of all your public keys or create a new key.\n"
           "\n"
           "Each public key can be used for signing securely 262144 (64^3) times.\n"
           "\n"
           "action: (optional)\n"
           "    list : List your existing public keys. The default.\n"
           "    checkkeys : Checks if your Public and Private keys are correct.\n"
           "    new : Create a new key pair.\n"
           "\n"
           "publickey: (optional)\n"
           "    Search for a specific public key.\n"
           "\n"
           "Examples:\n"
           "\n"
           "keys\n"
           "\n"
           "keys action:list\n"
           "\n"
           "keys action:checkkeys\n"
           "\n"
           "keys action:list publickey:0xFFEE56..\n"
           "\n"
           "keys action:new\n";
}

std::vector<std::string> keys::getValidParams() const {
    return std::vector<std::string>{ "action", "publickey", "phrase", "modifier" };
}

// THIS IS THE NEW HELPER FUNCTION
static JSONObject makeKeyJSON(const KeyRow& kr) {
    JSONObject dets;
    std::string pub0x = MiniData(kr.getPublicKey()).to0xString();
    std::string priv = kr.getPrivateKey();
    std::string priv0x = priv.empty() ? std::string("") : MiniData(priv).to0xString();

    dets.put("size", kr.getSize());
    dets.put("depth", kr.getDepth());
    dets.put("uses", kr.getUses());
    dets.put("maxuses", kr.getMaxUses());
    dets.put("modifier", kr.getModifier());
    dets.put("publickey", pub0x);
    dets.put("privatekey", priv0x);
    return dets;
}


std::unique_ptr<JSONObject> keys::runCommand() {
    auto ret = getJSONReply();

    // Get the wallet
    Wallet& wallet = MinimaDB::getDB()->getWallet();

    std::string action = getParam("action", "list");

    if (action == "list") {
        // ... (content is fine) ...
        bool searchkey = false;
        std::string pubkey = getParam("publickey", "");
        if (!pubkey.empty()) {
            pubkey = MiniData(pubkey).to0xString();
            searchkey = true;
        }

        bool searchmod = false;
        std::string modifier = getParam("modifier", "");
        if (!modifier.empty()) {
            searchmod = true;
        }

        // Get all the keys
        std::vector<std::unique_ptr<KeyRow>> keys = wallet.getAllKeys();

        JSONArray arr;
        int maxuses = 0;
        for (const auto& kptr : keys) {
            const KeyRow& kr = *kptr;

            // Get the details
            // THIS FUNCTION CALL IS NOW CORRECT
            JSONObject dets = makeKeyJSON(kr); 

            // Are we searching for ONE key or ALL of them
            if (searchkey) {
                if (dets.getString("publickey") == pubkey) {
                    if (kr.getUses() > maxuses) {
                        maxuses = kr.getUses();
                    }
                    arr.add(dets);
                    break;
                }
            } else if (searchmod) {
                if (dets.getString("modifier") == modifier) {
                    if (kr.getUses() > maxuses) {
                        maxuses = kr.getUses();
                    }
                    arr.add(dets);
                    break;
                }
            } else {
                if (kr.getUses() > maxuses) {
                    maxuses = kr.getUses();
                }
                arr.add(dets);
            }
        }

        JSONObject resp;
        resp.put("keys", arr);
        resp.put("total", static_cast<int>(arr.size()));
        resp.put("maxuses", maxuses);

        // Put the details in the response
        ret->put("response", resp);

    } else if (action == "genkey") {
        // ... (content is fine) ...
        std::string passphrase;
        if (!existsParam("phrase")) {
            // Create a new seed phrase
            std::vector<std::string> words = BIP39::getNewWordList();

            // Generate a new KEY and Passphrase
            passphrase = BIP39::convertWordListToString(words);
        } else {
            passphrase = getParam("phrase");
        }
        MiniData seed = BIP39::convertStringToSeed(passphrase);
        MiniData modifier_md("0x00");
        MiniData privseed = Crypto::getInstance().hashObjects(seed, modifier_md);
        TreeKey treekey = TreeKey::createDefault(privseed);
        std::string script = std::string("RETURN SIGNEDBY(") + treekey.getPublicKey().to0xString() + ")";
        Address newaddress(script);
        JSONObject resp;
        resp.put("phrase", passphrase);
        resp.put("privatekey", treekey.getPrivateKey().to0xString());
        resp.put("modifier", 0);
        resp.put("publickey", treekey.getPublicKey().to0xString());
        resp.put("script", script);
        resp.put("address", newaddress.getAddressData().to0xString());
        resp.put("miniaddress", Address::makeMinimaAddress(newaddress.getAddressData()));
        ret->put("response", resp);

    } else if (action == "checkkeys") {
        // ... (content is fine) ...
        if (!MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
            throw org::minima::system::commands::CommandException("Cannot check keys of locked DB..");
        }
        std::vector<std::unique_ptr<KeyRow>> keys = wallet.getAllKeys();
        int correct = 0;
        int wrong = 0;
        for (const auto& kptr : keys) {
            const KeyRow& kr = *kptr;
            TreeKey tk(MiniData(kr.getPrivateKey()), kr.getSize(), kr.getDepth());
            MiniData pubk(kr.getPublicKey());
            MiniData actualkey = tk.getPublicKey();
            if (!pubk.isEqual(actualkey)) {
                MinimaLogger::log(std::string("[!] INCORRECT Public key : ") + pubk.toString() + " / " + actualkey.toString());
                wrong++;
            } else {
                MinimaLogger::log(std::string("CORRECT Public key : ") + pubk.toString());
                correct++;
            }
        }
        JSONObject resp;
        resp.put("allkeys", static_cast<int>(keys.size()));
        resp.put("correct", correct);
        resp.put("wrong", wrong);
        ret->put("response", resp);

    } else if (action == "new") {
        // Create a new Key..
        std::unique_ptr<KeyRow> krow = wallet.createNewKey();
        // THIS FUNCTION CALL IS NOW CORRECT
        JSONObject jobj = makeKeyJSON(*krow); 
        ret->put("response", jobj);

    } else {
        throw org::minima::system::commands::CommandException(std::string("Unknown action : ") + action);
    }

    return ret;
}

bool keys::checkKey(const std::string& zPublicKey) {
    // ... (content is fine) ...
    MiniData pubkey(zPublicKey);
    Wallet& w = MinimaDB::getDB()->getWallet();
    std::unique_ptr<KeyRow> kr = w.getKeyFromPublic(zPublicKey);
    if (!kr) {
        return false;
    }
    if (!w.checkSingleKey(kr->getPrivateKey(), kr->getModifier())) {
        return false;
    }
    TreeKey tk(MiniData(kr->getPrivateKey()), kr->getSize(), kr->getDepth());
    MiniData actualkey = tk.getPublicKey();
    if (!pubkey.isEqual(actualkey)) {
        return false;
    }
    return true;
}

org::minima::system::commands::Command* keys::getFunction() {
    return new keys();
}

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org