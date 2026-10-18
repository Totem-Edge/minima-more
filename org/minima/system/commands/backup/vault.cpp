#include "org/minima/system/commands/backup/vault.hpp"

#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>
#include <filesystem>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp" // For setEncryptedSeed/getEncryptedSeed
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/keys/tree_key.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/b_i_p39.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/encrypt/password_crypto.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/ssl/s_s_l_manager.hpp"
#include "org/minima/database/wallet/seed_row.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniString;

// Helper: lowercase copy
static std::string toLowerCopy(const std::string& s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return out;
}

// Helper: encode a non-negative integer into minimal unsigned big-endian bytes
static std::vector<std::uint8_t> encodeUnsignedBigEndian(std::uint64_t v) {
    if (v == 0) {
        return std::vector<std::uint8_t>{0x00};
    }
    std::vector<std::uint8_t> bytes;
    while (v > 0) {
        bytes.push_back(static_cast<std::uint8_t>(v & 0xFF));
        v >>= 8;
    }
    std::reverse(bytes.begin(), bytes.end());
    while (bytes.size() > 1 && bytes[0] == 0x00) {
        bytes.erase(bytes.begin());
    }
    return bytes;
}

vault::vault()
    : org::minima::system::commands::Command(
          "vault",
          "[action:seed|wipekeys|restorekeys|passwordlock|passwordunlock] (seed:) (phrase:) - BE CAREFUL. Wipe/Restore/Encrypt/Decrypt your private keys") {}

std::string vault::getFullHelp() const {
    return
        "\nvault\n"
        "\n"
        "BE CAREFUL. Wipe / Restore your private keys.\n"
        "\n"
        "View your passphrase and seed. DO NOT SHARE THESE WITH ANYONE.\n"
        "\n"
        "ENSURE YOU HAVE A BACKUP AND SECURE RECORD OF YOUR PASSPHRASE BEFORE LOCKING.\n"
        "\n"
        "You must have your passphrase to unlock your private keys.\n"
        "\n"
        "action: (optional)\n"
        "    seed : Show your seed phrase. The default. \n"
        "    wipekeys : Wipe your private keys - keep the public.\n"
        "    restorekeys : Restore your private keys.\n"
        "    passwordlock : Lock your node by password encrypting private keys.\n"
        "    passwordunlock : Unlock your node to reinstating your private keys.\n"
        "\n"
        "seed: (optional)\n"
        "    Enter your seed to lock your node.\n"
        "    This will delete your private keys.\n"
        "\n"
        "phrase: (optional)\n"
        "    Enter your passphrase in double quotes to restore your node.\n"
        "    This will reinstate your private keys.\n"
        "\n"
        "Examples:\n"
        "\n"
        "vault\n"
        "\n"
        "vault action:wipekeys seed:0xDD4E..\n"
        "\n"
        "vault action:restorekeys phrase:\"SPRAY LAMP..\"\n"
        "\n"
        "vault action:passwordlock password:your_password\n"
        "\n"
        "vault action:passwordlock password:your_password confirm:your_password\n"
        "\n"
        "vault action:passwordunlock password:your_password\n";
}

std::vector<std::string> vault::getValidParams() const {
    return std::vector<std::string>{ "action","seed","keyuses","phrase","password","confirm","numkeys" };
}

std::unique_ptr<JSONObject> vault::runCommand() {
    std::unique_ptr<JSONObject> ret = getJSONReply();

    std::string action = getParam("action", "seed");

    //
    // FIX 1: getBaseSeed() returns an object, not a pointer. Use .
    //
    org::minima::database::wallet::SeedRow base = org::minima::database::MinimaDB::getDB()->getWallet().getBaseSeed();

    if (action == "seed") {
        JSONObject json;
        //
        // FIX 1: Use . operator
        //
        json.put("phrase", base.getPhrase());
        json.put("seed", base.getSeed());
        
        //
        // FIX 2: getWallet() returns a reference, use .
        //
        json.put("locked", !org::minima::database::MinimaDB::getDB()->getWallet().isBaseSeedAvailable());
        ret->put("response", json);

    } else if (action == "wipekeys") {
        checkAllKeysCreated();

        //
        // FIX 2: Use . operator
        //
        if (!org::minima::database::MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
            throw org::minima::system::commands::CommandException("DB already locked!");
        }

        std::unique_ptr<MiniData> seed = getDataParam("seed");
        if (!seed) {
            throw org::minima::system::commands::CommandException("Missing seed");
        }
        //
        // FIX 1: Use . operator
        //
        if (seed->to0xString() != base.getSeed()) {
            throw org::minima::system::commands::CommandException("Incorrect seed for lock");
        }

        //
        // FIX 2: Use . operator
        //
        org::minima::database::MinimaDB::getDB()->getWallet().wipeBaseSeedRow();

        ret->put("response", std::string("All private keys wiped!"));

    } else if (action == "restorekeys") {
        std::string initphrase = getParam("phrase");

        std::string cleanphrase = org::minima::utils::BIP39::cleanSeedPhrase(initphrase);

        MiniData seed = org::minima::utils::BIP39::convertStringToSeed(cleanphrase);

        //
        // FIX 2: Use . operator
        //
        bool ok = org::minima::database::MinimaDB::getDB()->getWallet().resetBaseSeedPrivKeys(cleanphrase, seed.to0xString());
        if (!ok) {
            throw org::minima::system::commands::CommandException("Error updating Private keys.. please try again");
        }

        JSONObject resp;
        resp.put("entered", initphrase);
        resp.put("cleaned", cleanphrase);
        resp.put("same", toLowerCopy(cleanphrase) == toLowerCopy(initphrase));
        resp.put("result", std::string("All private keys restored!"));
        ret->put("response", resp);

    } else if (action == "passwordlock") {
        checkAllKeysCreated();

        std::string password = getParam("password");
        if (password.find(';') != std::string::npos) {
            throw org::minima::system::commands::CommandException("Cannot use ; in password");
        }

        if (existsParam("confirm")) {
            std::string confirm = getParam("confirm");
            if (password != confirm) {
                throw org::minima::system::commands::CommandException("Passwords do NOT match!");
            }
        }

        passwordLockDB(password);

        ret->put("response", std::string("All private keys wiped! Stored encrypted in UserDB"));

    } else if (action == "passwordunlock") {
        std::string password = getParam("password");

        passowrdUnlockDB(password);

        ret->put("response", std::string("All private keys restored!"));

    } else if (action == "testphrase") {
        std::string initphrase = getParam("phrase");

        std::string cleanphrase = org::minima::utils::BIP39::cleanSeedPhrase(initphrase);

        MiniData seed = org::minima::utils::BIP39::convertStringToSeed(cleanphrase);

        int numkeys = getNumberParam("numkeys", org::minima::objects::base::MiniNumber(4))->getAsInt();

        JSONArray arr;
        for (int i = 0; i < numkeys; ++i) {
            org::minima::utils::MinimaLogger::log(std::string("Creating key : ") + std::to_string(i));

            MiniData modifier(encodeUnsignedBigEndian(static_cast<std::uint64_t>(i)));

            MiniData privseed = org::minima::utils::Crypto::getInstance().hashObjects(seed, modifier);

            org::minima::objects::keys::TreeKey treekey = org::minima::objects::keys::TreeKey::createDefault(privseed);

            std::string script = std::string("RETURN SIGNEDBY(") + treekey.getPublicKey().toString() + ")";

            org::minima::objects::Address addr(script);

            JSONObject json;
            json.put("publickey", treekey.getPublicKey().to0xString());
            json.put("address", addr.getMinimaAddress());

            arr.add(json);
        }

        JSONObject json;
        json.put("phrase", cleanphrase);
        json.put("seed", seed.to0xString());
        json.put("address", arr);

        ret->put("response", json);

    } else if (action == "resetkeys") {
        std::string phrase = getParam("phrase");

        int keyuses = getNumberParam("keyuses", org::minima::objects::base::MiniNumber(100))->getAsInt();

        std::string cleanphrase = org::minima::utils::BIP39::cleanSeedPhrase(phrase);

        org::minima::system::Main::getInstance()->restoreReady();

        //
        // FIX 5: Convert filesystem::path to std::string using .string()
        //
        std::string basedb = org::minima::database::MinimaDB::getDB()->getBaseDBFolder().string();
        std::filesystem::path basePath(basedb);
        try {
            org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER,
                                                             (basePath / "cascade.db").string());
            org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER,
                                                             (basePath / "chaintree.db").string());
            org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER,
                                                             (basePath / "userprefs.db").string());
            org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER,
                                                             (basePath / "p2p.db").string());
            // Remove SSL keystore folder
            //
            // THIS IS THE FIX: Removed the extra .string()
            //
            org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER,
                                                             org::minima::utils::ssl::SSLManager::getSSLFolder());
        } catch (const std::exception&){ /* ignore deletion errors to mimic Java behavior */ }

        //
        // FIX 3: Change from pointer (Wallet*) to reference (Wallet&)
        //
        org::minima::database::wallet::Wallet& wallet = org::minima::database::MinimaDB::getDB()->getWallet();

        //
        // FIX 3: Use . operator
        //
        wallet.resetDB(cleanphrase);

        wallet.initDefaultKeys(org::minima::database::wallet::Wallet::NUMBER_GETADDRESS_KEYS, true);

        wallet.updateAllKeyUses(keyuses);

        org::minima::database::MinimaDB::getDB()->saveSQL(false);

        org::minima::system::Main::getInstance()->setHasShutDown();
        // Java calls stopMessageProcessor(); not available in provided C++ Main

        ret->put("response", std::string("All private keys restored!"));

    } else {
        throw org::minima::system::commands::CommandException(std::string("Invalid action : ") + action);
    }

    return ret;
}

void vault::checkAllKeysCreated() {
    if (org::minima::system::Main::getInstance()->getAllKeysCreated()) {
        return;
    }
    throw org::minima::system::commands::CommandException(
        std::string("Please wait for ALL your keys to be created. ")
        + "This can take 5 mins. "
        + "Currently (" + std::to_string(org::minima::system::Main::getInstance()->getAllDefaultKeysSize())
        + "/" + std::to_string(org::minima::database::wallet::Wallet::NUMBER_GETADDRESS_KEYS) + ")"
    );
}

void vault::stopAllKeysCreated() {
    if (org::minima::system::Main::getInstance()->getAllKeysCreated()) {
        return;
    }

    //
    // FIX 2: Use . operator
    //
    org::minima::database::MinimaDB::getDB()->getWallet().setStopNewKeys(true);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

void vault::passwordLockDB(const std::string& zPassword) {
    //
    // FIX 1: Use . operator
    //
    org::minima::database::wallet::SeedRow base = org::minima::database::MinimaDB::getDB()->getWallet().getBaseSeed();

    //
    // FIX 2: Use . operator
    //
    if (!org::minima::database::MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
        throw org::minima::system::commands::CommandException("DB already locked!");
    }

    //
    // FIX 1: Use . operator
    //
    MiniString phrase(base.getPhrase());

    MiniData data(phrase.getData());

    MiniData encrypted;
    try {
        encrypted = org::minima::utils::encrypt::PasswordCrypto::encryptPassword(zPassword, data);
    } catch (const std::exception& e) {
        throw org::minima::system::commands::CommandException(e.what());
    }

    //
    // FIX 4: getUserDB() returns a reference, use .
    //
    org::minima::database::MinimaDB::getDB()->getUserDB().setEncryptedSeed(encrypted);

    org::minima::database::MinimaDB::getDB()->saveUserDB();

    try {
        //
        // FIX 2: Use . operator
        //
        org::minima::database::MinimaDB::getDB()->getWallet().wipeBaseSeedRow();
    } catch (const std::exception& e) {
        throw org::minima::system::commands::CommandException(e.what());
    }
}

void vault::passowrdUnlockDB(const std::string& zPassword) {
    //
    // FIX 2: Use . operator
    //
    if (org::minima::database::MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
        throw org::minima::system::commands::CommandException("DB already unlocked!");
    }

    //
    // FIX 4: Use . operator
    //
    MiniData encrypted = org::minima::database::MinimaDB::getDB()->getUserDB().getEncryptedSeed();

    MiniData decrypt;
    try {
        decrypt = org::minima::utils::encrypt::PasswordCrypto::decryptPassword(zPassword, encrypted);
    } catch (const std::exception&) {
        throw org::minima::system::commands::CommandException("Incorrect password!");
    }

    std::string initphrase = MiniString(decrypt.getBytes()).toString();

    MiniData seed = org::minima::utils::BIP39::convertStringToSeed(initphrase);

    //
    // FIX 2: Use . operator
    //
    bool ok = org::minima::database::MinimaDB::getDB()->getWallet().resetBaseSeedPrivKeys(initphrase, seed.to0xString());
    if (!ok) {
        throw org::minima::system::commands::CommandException("Error updating Private keys.. please try again");
    }

    //
    // FIX 4: Use . operator
    //
    org::minima::database::MinimaDB::getDB()->getUserDB().setEncryptedSeed(MiniData::ZERO_TXPOWID());

    org::minima::database::MinimaDB::getDB()->saveUserDB();
}

org::minima::system::commands::Command* vault::getFunction() {
    return new vault();
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org

