#include "org/minima/database/wallet/wallet.hpp"

#include <stdexcept>
#include <sstream>
#include <openssl/rand.h>
#include <random>
#include <algorithm>
#include <filesystem>
#include <stdio.h>
#include <cstddef>

// SQLite C API
#include <sqlite3.h>

// Minima project includes
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"
#include "org/minima/system/commands/send/multisig.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/b_i_p39.hpp"
#include "org/minima/utils/crypto.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace database {
namespace wallet {

using org::minima::utils::MinimaLogger;

// Static init
int Wallet::NUMBER_GETADDRESS_KEYS = 64;

static void throwOnSQLiteError(int rc, sqlite3* db, const char* ctx) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE) {
        std::string msg = std::string("[SQLite] ") + ctx + " failed: " + (db ? sqlite3_errmsg(db) : "unknown");
        throw std::runtime_error(msg);
    }
}

Wallet::Wallet() : org::minima::utils::SqlDB() {
    if (org::minima::system::params::GeneralParams::TEST_PARAMS) {
        NUMBER_GETADDRESS_KEYS = 8;
    }
}

Wallet::~Wallet() {
    try {
        finalizeStatements();
        closeSecondaryConnection();
    } catch (...) {
        // swallow in destructor
    }
}

void Wallet::shuttingDown() {
    mShuttingdown = true;
}

void Wallet::openSecondaryConnection() {
    if (mConn) {
        return;
    }
    const std::string path = getSQLFile();
    int rc = sqlite3_open(path.c_str(), &mConn);
    if (rc != SQLITE_OK) {
        std::string err = mConn ? sqlite3_errmsg(mConn) : "unknown";
        throw std::runtime_error(std::string("Failed to open secondary SQLite connection: ") + err);
    }
    org::minima::utils::SqlDB::applySQLiteTuning(mConn);
}

void Wallet::closeSecondaryConnection() {
    if (mConn) {
        sqlite3_close(mConn);
        mConn = nullptr;
    }
}

void Wallet::finalizeStatements() {
    auto fin = [](sqlite3_stmt*& s) {
        if (s) { sqlite3_finalize(s); s = nullptr; }
    };
    fin(STMT_CREATE_PUBLIC_KEY);
    fin(STMT_GET_KEY);
    fin(STMT_GET_ALL_KEYS);
    fin(STMT_UPDATE_KEY_USES);
    fin(STMT_UPDATE_ALL_KEY_USES);
    fin(STMT_UPDATE_INC_ALL_KEY_USES);

    fin(STMT_WIPE_PRIVATE_KEYS);
    fin(STMT_UPDATE_PRIVATE_KEYS);

    fin(STMT_ADD_SCRIPT);
    fin(STMT_REMOVE_SCRIPT);
    fin(STMT_LIST_ALL_SCRIPTS);
    fin(STMT_LIST_SIMPLE_SCRIPTS);
    fin(STMT_LIST_TRACK_SCRIPTS);
    fin(STMT_LIST_DEFAULT_SCRIPTS);
    fin(STMT_GET_SCRIPT);

    fin(STMT_SELECT_SEED);
    fin(STMT_INSERT_SEED);
    fin(STMT_UPDATE_SEED);
}

void Wallet::prepareStatements() {
    finalizeStatements();

    const char* SQL_CREATE_TABLE_KEYS =
        "CREATE TABLE IF NOT EXISTS keys ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  size INTEGER NOT NULL,"
        "  depth INTEGER NOT NULL,"
        "  uses INTEGER NOT NULL,"
        "  maxuses INTEGER NOT NULL,"
        "  modifier TEXT NOT NULL,"
        "  privatekey TEXT NOT NULL,"
        "  publickey TEXT NOT NULL"
        ");";

    const char* SQL_CREATE_TABLE_SCRIPTS =
        "CREATE TABLE IF NOT EXISTS scripts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  script TEXT NOT NULL,"
        "  address TEXT NOT NULL,"
        "  simple INTEGER NOT NULL,"
        "  defaultaddress INTEGER NOT NULL,"
        "  publickey TEXT NOT NULL,"
        "  track INTEGER NOT NULL"
        ");";

    const char* SQL_CREATE_TABLE_SEED =
        "CREATE TABLE IF NOT EXISTS seed ("
        "  id INTEGER NOT NULL UNIQUE,"
        "  phrase TEXT NOT NULL,"
        "  seed TEXT NOT NULL"
        ");";

    char* errmsg = nullptr;
    int rc = sqlite3_exec(mConn, SQL_CREATE_TABLE_KEYS, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("Create table keys failed: " + msg);
    }
    rc = sqlite3_exec(mConn, SQL_CREATE_TABLE_SCRIPTS, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("Create table scripts failed: " + msg);
    }
    rc = sqlite3_exec(mConn, SQL_CREATE_TABLE_SEED, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("Create table seed failed: " + msg);
    }

    // Prepare statements
    const char* Q_CREATE_PUBLIC_KEY =
        "INSERT OR IGNORE INTO keys (size, depth, uses, maxuses, modifier, privatekey, publickey) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_CREATE_PUBLIC_KEY, -1, &STMT_CREATE_PUBLIC_KEY, nullptr), mConn, "prepare CREATE_PUBLIC_KEY");

    const char* Q_GET_ALL_KEYS = "SELECT * FROM keys;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_GET_ALL_KEYS, -1, &STMT_GET_ALL_KEYS, nullptr), mConn, "prepare GET_ALL_KEYS");

    const char* Q_GET_KEY = "SELECT * FROM keys WHERE publickey=?;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_GET_KEY, -1, &STMT_GET_KEY, nullptr), mConn, "prepare GET_KEY");

    const char* Q_UPDATE_KEY_USES = "UPDATE keys SET uses=? WHERE publickey=?;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_UPDATE_KEY_USES, -1, &STMT_UPDATE_KEY_USES, nullptr), mConn, "prepare UPDATE_KEY_USES");

    const char* Q_UPDATE_ALL_KEY_USES = "UPDATE keys SET uses=?;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_UPDATE_ALL_KEY_USES, -1, &STMT_UPDATE_ALL_KEY_USES, nullptr), mConn, "prepare UPDATE_ALL_KEY_USES");

    const char* Q_UPDATE_INC_ALL_KEY_USES = "UPDATE keys SET uses=uses+?;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_UPDATE_INC_ALL_KEY_USES, -1, &STMT_UPDATE_INC_ALL_KEY_USES, nullptr), mConn, "prepare UPDATE_INC_ALL_KEY_USES");

    const char* Q_WIPE_PRIVATE_KEYS = "UPDATE keys SET privatekey='0x00' WHERE privatekey!='0x00';";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_WIPE_PRIVATE_KEYS, -1, &STMT_WIPE_PRIVATE_KEYS, nullptr), mConn, "prepare WIPE_PRIVATE_KEYS");

    const char* Q_UPDATE_PRIVATE_KEYS = "UPDATE keys SET privatekey=? WHERE publickey=?;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_UPDATE_PRIVATE_KEYS, -1, &STMT_UPDATE_PRIVATE_KEYS, nullptr), mConn, "prepare UPDATE_PRIVATE_KEYS");

    const char* Q_ADD_SCRIPT =
        "INSERT OR IGNORE INTO scripts (script, address, simple, defaultaddress, publickey, track) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_ADD_SCRIPT, -1, &STMT_ADD_SCRIPT, nullptr), mConn, "prepare ADD_SCRIPT");

    const char* Q_REMOVE_SCRIPT = "DELETE FROM scripts WHERE address=?;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_REMOVE_SCRIPT, -1, &STMT_REMOVE_SCRIPT, nullptr), mConn, "prepare REMOVE_SCRIPT");

    const char* Q_LIST_ALL_SCRIPTS = "SELECT * FROM scripts;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_LIST_ALL_SCRIPTS, -1, &STMT_LIST_ALL_SCRIPTS, nullptr), mConn, "prepare LIST_ALL_SCRIPTS");

    const char* Q_LIST_SIMPLE_SCRIPTS = "SELECT * FROM scripts WHERE simple<>0;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_LIST_SIMPLE_SCRIPTS, -1, &STMT_LIST_SIMPLE_SCRIPTS, nullptr), mConn, "prepare LIST_SIMPLE_SCRIPTS");

    const char* Q_LIST_TRACK_SCRIPTS = "SELECT * FROM scripts WHERE track<>0;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_LIST_TRACK_SCRIPTS, -1, &STMT_LIST_TRACK_SCRIPTS, nullptr), mConn, "prepare LIST_TRACK_SCRIPTS");

    const char* Q_LIST_DEFAULT_SCRIPTS = "SELECT * FROM scripts WHERE defaultaddress<>0;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_LIST_DEFAULT_SCRIPTS, -1, &STMT_LIST_DEFAULT_SCRIPTS, nullptr), mConn, "prepare LIST_DEFAULT_SCRIPTS");

    const char* Q_GET_SCRIPT = "SELECT * FROM scripts WHERE address=?;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_GET_SCRIPT, -1, &STMT_GET_SCRIPT, nullptr), mConn, "prepare GET_SCRIPT");

    const char* Q_SELECT_SEED = "SELECT * FROM seed WHERE id=1;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_SELECT_SEED, -1, &STMT_SELECT_SEED, nullptr), mConn, "prepare SELECT_SEED");

    const char* Q_INSERT_SEED = "INSERT INTO seed (id, phrase, seed) VALUES (1, ?, ?);";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_INSERT_SEED, -1, &STMT_INSERT_SEED, nullptr), mConn, "prepare INSERT_SEED");

    const char* Q_UPDATE_SEED = "UPDATE seed SET phrase=?, seed=? WHERE id=1;";
    throwOnSQLiteError(sqlite3_prepare_v2(mConn, Q_UPDATE_SEED, -1, &STMT_UPDATE_SEED, nullptr), mConn, "prepare UPDATE_SEED");
}

void Wallet::createSQL() {
    // Create tables/statements and initialize caches and base seed, and add multisig address
    openSecondaryConnection();
    prepareStatements();

    // Reset caches
    {
        mAllKeys.clear();
        mAllTrackedAddress.clear();
        mAllSimpleAddress.clear();
    }

    // Load caches from DB
    {
        auto keys = getAllKeys();
        // std::cout << "DEBUG: Loaded " << keys.size() << " keys from database" << std::endl;
        for (const auto& k : keys) {
            // std::cout << "DEBUG: Loading key: " << k->getPublicKey() << std::endl;
            mAllKeys.insert(k->getPublicKey());
        }
        // std::cout << "DEBUG: mAllKeys.size() after loading = " << mAllKeys.size() << std::endl;

        auto scripts = getAllAddresses();
        for (const auto& s : scripts) {
            const std::string& addr = s->getAddress();
            if (s->isTrack()) {
                mAllTrackedAddress.insert(addr);
            }
            if (s->isSimple()) {
                mAllSimpleAddress.insert(addr);
            }
        }
    }

    // Initialize base seed
    initBaseSeed();

    // Ensure base MULTISIG address exists
    std::string msaddress =
        org::minima::objects::Address(org::minima::system::commands::send::multisig::MULTISIG_CONTRACT)
            .getAddressData().to0xString();
    auto scr = getScriptFromAddress(msaddress);
    if (!scr) {
        org::minima::utils::MinimaLogger::log("Adding base MULTISIG address " + msaddress);
        addScript(org::minima::system::commands::send::multisig::MULTISIG_CONTRACT, false, false, "0x00", false);
    }
}

void Wallet::resetDB(const std::string& zNewSeedPhrase) {
    // Drop all known tables; re-open, then set new seed
    openSecondaryConnection();

    char* errmsg = nullptr;
    const char* DROP_KEYS = "DROP TABLE IF EXISTS keys;";
    const char* DROP_SCRIPTS = "DROP TABLE IF EXISTS scripts;";
    const char* DROP_SEED = "DROP TABLE IF EXISTS seed;";

    int rc = sqlite3_exec(mConn, DROP_KEYS, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("ResetDB drop keys failed: " + msg);
    }
    rc = sqlite3_exec(mConn, DROP_SCRIPTS, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("ResetDB drop scripts failed: " + msg);
    }
    rc = sqlite3_exec(mConn, DROP_SEED, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("ResetDB drop seed failed: " + msg);
    }

    // Close connections and reopen via SqlDB (will call createSQL)
    finalizeStatements();
    closeSecondaryConnection();
    hardCloseDB();
    checkOpen();

    // Convert phrase to seed and update
    org::minima::objects::base::MiniData seed = org::minima::utils::BIP39::convertStringToSeed(zNewSeedPhrase);
    updateSeedRow(zNewSeedPhrase, seed.to0xString());
}

void Wallet::initBaseSeed() {
    // Try to select the seed
    throwOnSQLiteError(sqlite3_reset(STMT_SELECT_SEED), mConn, "reset SELECT_SEED");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_SELECT_SEED), mConn, "clear SELECT_SEED");
    int rc = sqlite3_step(STMT_SELECT_SEED);
    if (rc == SQLITE_ROW) {
        // columns: id (0), phrase (1), seed (2)
        const unsigned char* phr = sqlite3_column_text(STMT_SELECT_SEED, 1);
        const unsigned char* sed = sqlite3_column_text(STMT_SELECT_SEED, 2);
        std::string phrase = phr ? reinterpret_cast<const char*>(phr) : "";
        std::string seed = sed ? reinterpret_cast<const char*>(sed) : "";
        mBaseSeed = std::make_unique<SeedRow>(phrase, seed);
        // done
        return;
    }
    // rc could be SQLITE_DONE (no rows) or an error
    throwOnSQLiteError(rc, mConn, "step SELECT_SEED");

    // Not found; generate a new one
    org::minima::utils::MinimaLogger::log("Generating Base Private Seed Key");

    std::string phrase;
    if (!org::minima::system::params::GeneralParams::SEED_PHRASE.empty()) {
        org::minima::utils::MinimaLogger::log("Using provided seed phrase from params..");
        if (!org::minima::system::params::GeneralParams::ANYSEED_PHRASE) {
            phrase = org::minima::utils::BIP39::cleanSeedPhrase(
                org::minima::system::params::GeneralParams::SEED_PHRASE);
        } else {
            phrase = org::minima::system::params::GeneralParams::SEED_PHRASE;
        }
        // wipe param
        org::minima::system::params::GeneralParams::SEED_PHRASE.clear();
    } else {
        auto words = org::minima::utils::BIP39::getNewWordList();
        phrase = org::minima::utils::BIP39::convertWordListToString(words);
    }

    org::minima::objects::base::MiniData seed = org::minima::utils::BIP39::convertStringToSeed(phrase);

    // Insert
    throwOnSQLiteError(sqlite3_reset(STMT_INSERT_SEED), mConn, "reset INSERT_SEED");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_INSERT_SEED), mConn, "clear INSERT_SEED");
    throwOnSQLiteError(sqlite3_bind_text(STMT_INSERT_SEED, 1, phrase.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind INSERT_SEED phrase");
    std::string seedhex = seed.to0xString();
    throwOnSQLiteError(sqlite3_bind_text(STMT_INSERT_SEED, 2, seedhex.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind INSERT_SEED seed");
    throwOnSQLiteError(sqlite3_step(STMT_INSERT_SEED), mConn, "step INSERT_SEED");

    mBaseSeed = std::make_unique<SeedRow>(phrase, seedhex);
}

void Wallet::updateSeedRow(const std::string& zPhrase, const std::string& zSeed) {
    throwOnSQLiteError(sqlite3_reset(STMT_UPDATE_SEED), mConn, "reset UPDATE_SEED");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_UPDATE_SEED), mConn, "clear UPDATE_SEED");
    throwOnSQLiteError(sqlite3_bind_text(STMT_UPDATE_SEED, 1, zPhrase.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind UPDATE_SEED phrase");
    throwOnSQLiteError(sqlite3_bind_text(STMT_UPDATE_SEED, 2, zSeed.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind UPDATE_SEED seed");
    throwOnSQLiteError(sqlite3_step(STMT_UPDATE_SEED), mConn, "step UPDATE_SEED");

    mBaseSeed = std::make_unique<SeedRow>(zPhrase, zSeed);
}

SeedRow Wallet::getBaseSeed() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!mBaseSeed) {
        throw std::runtime_error("Base seed not initialized");
    }
    return *mBaseSeed;
}

bool Wallet::isBaseSeedAvailable() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!mBaseSeed) return false;
    return mBaseSeed->getSeed() != "0x00";
}

void Wallet::wipeBaseSeedRow() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Wipe private keys
    throwOnSQLiteError(sqlite3_reset(STMT_WIPE_PRIVATE_KEYS), mConn, "reset WIPE_PRIVATE_KEYS");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_WIPE_PRIVATE_KEYS), mConn, "clear WIPE_PRIVATE_KEYS");
    throwOnSQLiteError(sqlite3_step(STMT_WIPE_PRIVATE_KEYS), mConn, "step WIPE_PRIVATE_KEYS");
    
    // Update seed to locked state
    throwOnSQLiteError(sqlite3_reset(STMT_UPDATE_SEED), mConn, "reset UPDATE_SEED");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_UPDATE_SEED), mConn, "clear UPDATE_SEED");
    throwOnSQLiteError(sqlite3_bind_text(STMT_UPDATE_SEED, 1, "", -1, SQLITE_TRANSIENT), mConn, "bind UPDATE_SEED empty phrase");
    throwOnSQLiteError(sqlite3_bind_text(STMT_UPDATE_SEED, 2, "0x00", -1, SQLITE_TRANSIENT), mConn, "bind UPDATE_SEED zero seed");
    throwOnSQLiteError(sqlite3_step(STMT_UPDATE_SEED), mConn, "step UPDATE_SEED");
    
    //  Finalize statements BEFORE closing connection
    finalizeStatements();
    
    // Now save and reopen
    saveDB(true);
    checkOpen();
    
    //  Rebuild prepared statements for the new connection
    prepareStatements();
    
    mBaseSeed = std::make_unique<SeedRow>("", "0x00");
}

bool Wallet::resetBaseSeedPrivKeys(const std::string& zPhrase, const std::string& zSeed) {
    try {
        updateSeedRow(zPhrase, zSeed);

        auto keys = getAllKeys();
        org::minima::objects::base::MiniData seed(zSeed);

        for (const auto& key : keys) {
            org::minima::objects::base::MiniData modifier(key->getModifier());
            org::minima::objects::base::MiniData privseed =
                org::minima::utils::Crypto::getInstance().hashObjects(seed, modifier);

            // Update DB
            throwOnSQLiteError(sqlite3_reset(STMT_UPDATE_PRIVATE_KEYS), mConn, "reset UPDATE_PRIVATE_KEYS");
            throwOnSQLiteError(sqlite3_clear_bindings(STMT_UPDATE_PRIVATE_KEYS), mConn, "clear UPDATE_PRIVATE_KEYS");
            std::string privhex = privseed.to0xString();
            throwOnSQLiteError(sqlite3_bind_text(STMT_UPDATE_PRIVATE_KEYS, 1, privhex.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind UPDATE_PRIVATE_KEYS privatekey");
            throwOnSQLiteError(sqlite3_bind_text(STMT_UPDATE_PRIVATE_KEYS, 2, key->getPublicKey().c_str(), -1, SQLITE_TRANSIENT), mConn, "bind UPDATE_PRIVATE_KEYS publickey");
            throwOnSQLiteError(sqlite3_step(STMT_UPDATE_PRIVATE_KEYS), mConn, "step UPDATE_PRIVATE_KEYS");
        }
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return false;
    }
    return true;
}

bool Wallet::checkAllPrivateKeys() {
    auto allkeys = getAllKeys();
    bool allok = true;
    for (const auto& kr : allkeys) {
        if (!checkSingleKey(kr->getPrivateKey(), kr->getModifier())) {
            org::minima::utils::MinimaLogger::log("[SERIOUS ERROR] Private key NOT == Seed+Modifier publickey:" + kr->getPublicKey());
            allok = false;
        }
    }
    return allok;
}

bool Wallet::checkSingleKey(const std::string& zPrivateKey, const std::string& zModifier) {
    org::minima::objects::base::MiniData privatekey(zPrivateKey);
    org::minima::objects::base::MiniData mod(zModifier);
    std::string seedhex;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!mBaseSeed) return false;
        seedhex = mBaseSeed->getSeed();
    }
    org::minima::objects::base::MiniData seedmd(seedhex);
    org::minima::objects::base::MiniData privseed =
        org::minima::utils::Crypto::getInstance().hashObjects(seedmd, mod);
    if (!privseed.isEqual(privatekey)) {
        return false;
    }
    return true;
}

void Wallet::setStopNewKeys(bool zStopNewKeys) {
    mStopNewKeys = zStopNewKeys;
    if (mStopNewKeys) {
        org::minima::utils::MinimaLogger::log("Disallow creation of NEW default keys..");
    } else {
        org::minima::utils::MinimaLogger::log("Re-allow creation of NEW default keys..");
    }
}

int Wallet::getDefaultKeysNumber() {
    return static_cast<int>(getAllDefaultAddresses().size());
}

bool Wallet::initDefaultKeys(int zMaxNum) {
    return initDefaultKeys(zMaxNum, false);
}

bool Wallet::initDefaultKeys(int zMaxNum, bool zLog) {
    if (mShuttingdown || mStopNewKeys) {
        return true;
    }

    auto allscripts = getAllDefaultAddresses();
    bool allcreated = false;

    int numkeys = static_cast<int>(allscripts.size());
    if (numkeys < NUMBER_GETADDRESS_KEYS) {
        int diff = NUMBER_GETADDRESS_KEYS - numkeys;
        if (diff > zMaxNum) diff = zMaxNum;

        for (int i = 0; i < diff; ++i) {
            if (!mShuttingdown && !mStopNewKeys) {
                createNewSimpleAddress(true);
                if (zLog) {
                    std::ostringstream os;
                    os << "Default key created.. total:" << (numkeys + i + 1);
                    org::minima::utils::MinimaLogger::log(os.str());
                }
            }
        }
        {
            std::ostringstream os;
            os << diff << " more initial keys created.. Total now : " << (numkeys + diff)
               << " / " << NUMBER_GETADDRESS_KEYS;
            org::minima::utils::MinimaLogger::log(os.str());
        }
    } else {
        allcreated = true;
    }

    if (static_cast<int>(getAllDefaultAddresses().size()) >= NUMBER_GETADDRESS_KEYS) {
        return true;
    }
    return allcreated;
}

std::unique_ptr<ScriptRow> Wallet::getDefaultAddress() {
    auto allkeys = getAllDefaultAddresses();
    int numkeys = static_cast<int>(allkeys.size());
    if (numkeys == 0) return nullptr;

    // Cryptographically secure selection of a default address index (was mt19937).
    unsigned char rbuf[4];
    if (RAND_bytes(rbuf, 4) != 1) {
        throw std::runtime_error("RAND_bytes failed (wallet address selection)");
    }
    std::size_t rv = (std::size_t(rbuf[0]) << 24) | (std::size_t(rbuf[1]) << 16) |
                     (std::size_t(rbuf[2]) << 8) | std::size_t(rbuf[3]);
    int idx = static_cast<int>(rv % static_cast<std::size_t>(numkeys));

    // Return a copy
    const ScriptRow& s = *allkeys[idx];
    return std::make_unique<ScriptRow>(s.getScript(), s.getAddress(), s.isSimple(), s.isDefault(), s.getPublicKey(), s.isTrack());
}

std::unique_ptr<ScriptRow> Wallet::createNewSimpleAddress(bool zDefault) {
    auto key = createNewKey();
    if (!key) return nullptr;

    std::string script = std::string("RETURN SIGNEDBY(") + key->getPublicKey() + ")";
    return addScript(script, true, zDefault, key->getPublicKey(), true);
}

org::minima::objects::base::MiniData Wallet::miniDataFromUInt64(std::uint64_t v) {
    // Convert to BigInteger-like format (with potential leading zero)
    std::vector<std::uint8_t> bytes;
    
    // Convert to bytes (extract byte by byte)
    std::vector<std::uint8_t> temp;
    if (v == 0) {
        temp.push_back(0);
    } else {
        while (v > 0) {
            temp.push_back(static_cast<std::uint8_t>(v & 0xFF));
            v >>= 8;
        }
        std::reverse(temp.begin(), temp.end());
    }
    
    // Add leading zero byte if MSB is set (sign bit, matching Java's BigInteger.toByteArray())
    if (!temp.empty() && (temp[0] & 0x80)) {
        bytes.push_back(0x00);
    }
    bytes.insert(bytes.end(), temp.begin(), temp.end());
    
    return org::minima::objects::base::MiniData(bytes);
}

std::unique_ptr<KeyRow> Wallet::createNewKey() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::uint64_t numkeys = mAllKeys.size();
    
    // ===== DIAGNOSTIC CODE START =====
    // std::cout << "\n=== CREATING KEY: numkeys = " << numkeys << " ===" << std::endl;
    
    // org::minima::objects::base::MiniData modifier = miniDataFromUInt64(numkeys);
    // std::cout << "Modifier: " << modifier.to0xString() << std::endl;
    
    // std::string seedhex = mBaseSeed->getSeed();
    // org::minima::objects::base::MiniData seedmd(seedhex);
    // std::cout << "Base seed: " << seedmd.to0xString().substr(0, 20) << "..." << std::endl;
    
    // org::minima::objects::base::MiniData privseed = 
    //     org::minima::utils::Crypto::getInstance().hashObjects(seedmd, modifier);
    // std::cout << "Private seed (hash of base+modifier): " << privseed.to0xString() << std::endl;
    
    // org::minima::objects::keys::TreeKey treekey = org::minima::objects::keys::TreeKey::createDefault(privseed);
    // org::minima::objects::base::MiniData pubkey = treekey.getPublicKey();
    // std::cout << "Generated public key: " << pubkey.to0xString() << std::endl;
    
    // // Check if this public key already exists
    // std::string pubhex = pubkey.to0xString();
    // bool alreadyExists = (mAllKeys.find(pubhex) != mAllKeys.end());
    // std::cout << "Public key already in cache? " << (alreadyExists ? "YES âŒ" : "NO âœ“") << std::endl;
    // ===== DIAGNOSTIC CODE END =====
    
    org::minima::objects::base::MiniData modifier = miniDataFromUInt64(numkeys);

    // private seed = hash(BaseSeed, modifier)
    std::string seedhex = mBaseSeed->getSeed();
    org::minima::objects::base::MiniData seedmd(seedhex);
    org::minima::objects::base::MiniData privseed =
        org::minima::utils::Crypto::getInstance().hashObjects(seedmd, modifier);

    // // TreeKey
    org::minima::objects::keys::TreeKey treekey = org::minima::objects::keys::TreeKey::createDefault(privseed);

    // Insert into DB
    throwOnSQLiteError(sqlite3_reset(STMT_CREATE_PUBLIC_KEY), mConn, "reset CREATE_PUBLIC_KEY");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_CREATE_PUBLIC_KEY), mConn, "clear CREATE_PUBLIC_KEY");
    throwOnSQLiteError(sqlite3_bind_int(STMT_CREATE_PUBLIC_KEY, 1, treekey.getSize()), mConn, "bind CREATE_PUBLIC_KEY size");
    throwOnSQLiteError(sqlite3_bind_int(STMT_CREATE_PUBLIC_KEY, 2, treekey.getDepth()), mConn, "bind CREATE_PUBLIC_KEY depth");
    throwOnSQLiteError(sqlite3_bind_int(STMT_CREATE_PUBLIC_KEY, 3, treekey.getUses()), mConn, "bind CREATE_PUBLIC_KEY uses");
    throwOnSQLiteError(sqlite3_bind_int(STMT_CREATE_PUBLIC_KEY, 4, treekey.getMaxUses()), mConn, "bind CREATE_PUBLIC_KEY maxuses");

    std::string modhex = modifier.to0xString();
    std::string privhex = treekey.getPrivateKey().to0xString();
    std::string pubhex = treekey.getPublicKey().to0xString();

    throwOnSQLiteError(sqlite3_bind_text(STMT_CREATE_PUBLIC_KEY, 5, modhex.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind CREATE_PUBLIC_KEY modifier");
    throwOnSQLiteError(sqlite3_bind_text(STMT_CREATE_PUBLIC_KEY, 6, privhex.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind CREATE_PUBLIC_KEY privatekey");
    throwOnSQLiteError(sqlite3_bind_text(STMT_CREATE_PUBLIC_KEY, 7, pubhex.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind CREATE_PUBLIC_KEY publickey");

    throwOnSQLiteError(sqlite3_step(STMT_CREATE_PUBLIC_KEY), mConn, "step CREATE_PUBLIC_KEY");

    int changes = sqlite3_changes(mConn);
    if (changes == 0) {
        org::minima::utils::MinimaLogger::log("ERROR: INSERT OR IGNORE failed - key already exists in database!");
        org::minima::utils::MinimaLogger::log("ERROR: PublicKey: " + pubhex);
        org::minima::utils::MinimaLogger::log("ERROR: This means mAllKeys.size() is out of sync with the database!");
        throw std::runtime_error("Failed to insert key - already exists in database!");
    }

    // *** ONLY add to cache if insert succeeded ***
    // mAllKeys.insert(pubhex);
    [[maybe_unused]] auto result = mAllKeys.insert(pubhex);
    // std::cout << "DEBUG: Insert succeeded = " << result.second << std::endl;
    // std::cout << "DEBUG: mAllKeys.size() AFTER = " << mAllKeys.size() << std::endl;
    // org::minima::utils::MinimaLogger::log("DEBUG: Key created successfully. Cache size now = " + std::to_string(mAllKeys.size()));

    // Return KeyRow - Java used "0x00" for modifier in returned object (while DB stored true modifier)
    return std::make_unique<KeyRow>(
        treekey.getSize(), treekey.getDepth(),
        treekey.getUses(), treekey.getMaxUses(),
        "0x00",
        privhex,
        pubhex
    );
}

std::unique_ptr<ScriptRow> Wallet::addScript(const std::string& zScript,
                                             bool zSimple,
                                             bool zDefault,
                                             const std::string& zPublicKey,
                                             bool zTrack) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Compute address
    std::string addr = org::minima::objects::Address(zScript).getAddressData().to0xString();

    throwOnSQLiteError(sqlite3_reset(STMT_ADD_SCRIPT), mConn, "reset ADD_SCRIPT");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_ADD_SCRIPT), mConn, "clear ADD_SCRIPT");

    throwOnSQLiteError(sqlite3_bind_text(STMT_ADD_SCRIPT, 1, zScript.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind ADD_SCRIPT script");
    throwOnSQLiteError(sqlite3_bind_text(STMT_ADD_SCRIPT, 2, addr.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind ADD_SCRIPT address");
    throwOnSQLiteError(sqlite3_bind_int(STMT_ADD_SCRIPT, 3, zSimple ? 1 : 0), mConn, "bind ADD_SCRIPT simple");
    throwOnSQLiteError(sqlite3_bind_int(STMT_ADD_SCRIPT, 4, zDefault ? 1 : 0), mConn, "bind ADD_SCRIPT defaultaddress");
    throwOnSQLiteError(sqlite3_bind_text(STMT_ADD_SCRIPT, 5, zPublicKey.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind ADD_SCRIPT publickey");
    throwOnSQLiteError(sqlite3_bind_int(STMT_ADD_SCRIPT, 6, zTrack ? 1 : 0), mConn, "bind ADD_SCRIPT track");

    throwOnSQLiteError(sqlite3_step(STMT_ADD_SCRIPT), mConn, "step ADD_SCRIPT");

    if (zTrack) {
        mAllTrackedAddress.insert(addr);
    }
    if (zSimple) {
        mAllSimpleAddress.insert(addr);
    }

    return std::make_unique<ScriptRow>(zScript, addr, zSimple, zDefault, zPublicKey, zTrack);
}

void Wallet::removeScript(const std::string& zAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);

    throwOnSQLiteError(sqlite3_reset(STMT_REMOVE_SCRIPT), mConn, "reset REMOVE_SCRIPT");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_REMOVE_SCRIPT), mConn, "clear REMOVE_SCRIPT");
    throwOnSQLiteError(sqlite3_bind_text(STMT_REMOVE_SCRIPT, 1, zAddress.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind REMOVE_SCRIPT address");
    throwOnSQLiteError(sqlite3_step(STMT_REMOVE_SCRIPT), mConn, "step REMOVE_SCRIPT");
}

std::unique_ptr<ScriptRow> Wallet::getScriptFromAddress(const std::string& zAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);

    throwOnSQLiteError(sqlite3_reset(STMT_GET_SCRIPT), mConn, "reset GET_SCRIPT");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_GET_SCRIPT), mConn, "clear GET_SCRIPT");
    throwOnSQLiteError(sqlite3_bind_text(STMT_GET_SCRIPT, 1, zAddress.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind GET_SCRIPT address");

    int rc = sqlite3_step(STMT_GET_SCRIPT);
    if (rc == SQLITE_ROW) {
        // columns: id(0), script(1), address(2), simple(3), defaultaddress(4), publickey(5), track(6)
        const char* script = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_SCRIPT, 1));
        const char* address = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_SCRIPT, 2));
        int simple = sqlite3_column_int(STMT_GET_SCRIPT, 3);
        int defaddr = sqlite3_column_int(STMT_GET_SCRIPT, 4);
        const char* pubkey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_SCRIPT, 5));
        int track = sqlite3_column_int(STMT_GET_SCRIPT, 6);

        return std::make_unique<ScriptRow>(
            script ? script : "",
            address ? address : "",
            simple != 0,
            defaddr != 0,
            pubkey ? pubkey : "",
            track != 0
        );
    }
    if (rc != SQLITE_DONE) {
        throwOnSQLiteError(rc, mConn, "step GET_SCRIPT");
    }
    return nullptr;
}

bool Wallet::isKeyRelevant(const std::string& zPublicKey) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return mAllKeys.find(zPublicKey) != mAllKeys.end();
}

bool Wallet::isAddressRelevant(const std::string& zAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return mAllTrackedAddress.find(zAddress) != mAllTrackedAddress.end();
}

bool Wallet::isAddressSimple(const std::string& zAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return mAllSimpleAddress.find(zAddress) != mAllSimpleAddress.end();
}

std::vector<std::unique_ptr<KeyRow>> Wallet::getAllKeys() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::unique_ptr<KeyRow>> allkeys;
    throwOnSQLiteError(sqlite3_reset(STMT_GET_ALL_KEYS), mConn, "reset GET_ALL_KEYS");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_GET_ALL_KEYS), mConn, "clear GET_ALL_KEYS");

    while (true) {
        int rc = sqlite3_step(STMT_GET_ALL_KEYS);
        if (rc == SQLITE_ROW) {
            // columns: id(0), size(1), depth(2), uses(3), maxuses(4), modifier(5), privatekey(6), publickey(7)
            int size = sqlite3_column_int(STMT_GET_ALL_KEYS, 1);
            int depth = sqlite3_column_int(STMT_GET_ALL_KEYS, 2);
            int uses = sqlite3_column_int(STMT_GET_ALL_KEYS, 3);
            int maxuses = sqlite3_column_int(STMT_GET_ALL_KEYS, 4);
            const char* modifier = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_ALL_KEYS, 5));
            const char* privatekey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_ALL_KEYS, 6));
            const char* publickey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_ALL_KEYS, 7));

            allkeys.push_back(std::make_unique<KeyRow>(
                size, depth, uses, maxuses,
                modifier ? modifier : "",
                privatekey ? privatekey : "",
                publickey ? publickey : ""
            ));
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throwOnSQLiteError(rc, mConn, "step GET_ALL_KEYS");
        }
    }
    return allkeys;
}

std::unique_ptr<KeyRow> Wallet::getKeyFromPublic(const std::string& zPublicKey) {
    std::lock_guard<std::mutex> lock(m_mutex);

    throwOnSQLiteError(sqlite3_reset(STMT_GET_KEY), mConn, "reset GET_KEY");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_GET_KEY), mConn, "clear GET_KEY");
    throwOnSQLiteError(sqlite3_bind_text(STMT_GET_KEY, 1, zPublicKey.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind GET_KEY publickey");

    int rc = sqlite3_step(STMT_GET_KEY);
    if (rc == SQLITE_ROW) {
        int size = sqlite3_column_int(STMT_GET_KEY, 1);
        int depth = sqlite3_column_int(STMT_GET_KEY, 2);
        int uses = sqlite3_column_int(STMT_GET_KEY, 3);
        int maxuses = sqlite3_column_int(STMT_GET_KEY, 4);
        const char* modifier = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_KEY, 5));
        const char* privatekey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_KEY, 6));
        const char* publickey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_KEY, 7));

        return std::make_unique<KeyRow>(
            size, depth, uses, maxuses,
            modifier ? modifier : "",
            privatekey ? privatekey : "",
            publickey ? publickey : ""
        );
    }
    if (rc != SQLITE_DONE) {
        throwOnSQLiteError(rc, mConn, "step GET_KEY");
    }
    return nullptr;
}

std::vector<std::unique_ptr<ScriptRow>> Wallet::getAllAddresses() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::unique_ptr<ScriptRow>> allscripts;
    throwOnSQLiteError(sqlite3_reset(STMT_LIST_ALL_SCRIPTS), mConn, "reset LIST_ALL_SCRIPTS");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_LIST_ALL_SCRIPTS), mConn, "clear LIST_ALL_SCRIPTS");

    while (true) {
        int rc = sqlite3_step(STMT_LIST_ALL_SCRIPTS);
        if (rc == SQLITE_ROW) {
            const char* script = reinterpret_cast<const char*>(sqlite3_column_text(STMT_LIST_ALL_SCRIPTS, 1));
            const char* address = reinterpret_cast<const char*>(sqlite3_column_text(STMT_LIST_ALL_SCRIPTS, 2));
            int simple = sqlite3_column_int(STMT_LIST_ALL_SCRIPTS, 3);
            int defaddr = sqlite3_column_int(STMT_LIST_ALL_SCRIPTS, 4);
            const char* publickey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_LIST_ALL_SCRIPTS, 5));
            int track = sqlite3_column_int(STMT_LIST_ALL_SCRIPTS, 6);

            allscripts.push_back(std::make_unique<ScriptRow>(
                script ? script : "", address ? address : "", simple != 0, defaddr != 0,
                publickey ? publickey : "", track != 0
            ));
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throwOnSQLiteError(rc, mConn, "step LIST_ALL_SCRIPTS");
        }
    }
    return allscripts;
}

std::vector<std::unique_ptr<ScriptRow>> Wallet::getAllDefaultAddresses() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::unique_ptr<ScriptRow>> allscripts;
    throwOnSQLiteError(sqlite3_reset(STMT_LIST_DEFAULT_SCRIPTS), mConn, "reset LIST_DEFAULT_SCRIPTS");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_LIST_DEFAULT_SCRIPTS), mConn, "clear LIST_DEFAULT_SCRIPTS");

    while (true) {
        int rc = sqlite3_step(STMT_LIST_DEFAULT_SCRIPTS);
        if (rc == SQLITE_ROW) {
            const char* script = reinterpret_cast<const char*>(sqlite3_column_text(STMT_LIST_DEFAULT_SCRIPTS, 1));
            const char* address = reinterpret_cast<const char*>(sqlite3_column_text(STMT_LIST_DEFAULT_SCRIPTS, 2));
            int simple = sqlite3_column_int(STMT_LIST_DEFAULT_SCRIPTS, 3);
            int defaddr = sqlite3_column_int(STMT_LIST_DEFAULT_SCRIPTS, 4);
            const char* publickey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_LIST_DEFAULT_SCRIPTS, 5));
            int track = sqlite3_column_int(STMT_LIST_DEFAULT_SCRIPTS, 6);

            allscripts.push_back(std::make_unique<ScriptRow>(
                script ? script : "", address ? address : "", simple != 0, defaddr != 0,
                publickey ? publickey : "", track != 0
            ));
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throwOnSQLiteError(rc, mConn, "step LIST_DEFAULT_SCRIPTS");
        }
    }
    return allscripts;
}

std::unique_ptr<org::minima::objects::keys::Signature>
Wallet::signData(const std::string& zPublicKey, const org::minima::objects::base::MiniData& zData) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!mBaseSeed || mBaseSeed->getSeed() == "0x00") {
            throw std::invalid_argument("KeysDB LOCKED. No Private Keys..");
        }
    }

    try {
        // Get the key row
        throwOnSQLiteError(sqlite3_reset(STMT_GET_KEY), mConn, "reset GET_KEY");
        throwOnSQLiteError(sqlite3_clear_bindings(STMT_GET_KEY), mConn, "clear GET_KEY");
        throwOnSQLiteError(sqlite3_bind_text(STMT_GET_KEY, 1, zPublicKey.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind GET_KEY publickey");

        int rc = sqlite3_step(STMT_GET_KEY);
        if (rc != SQLITE_ROW) {
            if (rc != SQLITE_DONE) {
                throwOnSQLiteError(rc, mConn, "step GET_KEY");
            }
            return nullptr;
        }

        int size = sqlite3_column_int(STMT_GET_KEY, 1);
        int depth = sqlite3_column_int(STMT_GET_KEY, 2);
        int uses = sqlite3_column_int(STMT_GET_KEY, 3);
        const char* privatekey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_KEY, 6));
        const char* publickey = reinterpret_cast<const char*>(sqlite3_column_text(STMT_GET_KEY, 7));
        (void)publickey;

        // Build TreeKey and set uses
        org::minima::objects::base::MiniData privmd(privatekey ? privatekey : "");
        org::minima::objects::keys::TreeKey tk(privmd, size, depth);
        tk.setUses(uses);

        // Sign
        auto signature = std::make_unique<org::minima::objects::keys::Signature>(tk.sign(zData));

        // Update uses from tk
        int newuses = tk.getUses();
        updateUsesInternal(newuses, zPublicKey);

        // Return directly - no second move!
        return signature;

    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return nullptr;
    }
}

void Wallet::updateUsesInternal(int zUses, const std::string& zPublicKey) {
    throwOnSQLiteError(sqlite3_reset(STMT_UPDATE_KEY_USES), mConn, "reset UPDATE_KEY_USES");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_UPDATE_KEY_USES), mConn, "clear UPDATE_KEY_USES");
    throwOnSQLiteError(sqlite3_bind_int(STMT_UPDATE_KEY_USES, 1, zUses), mConn, "bind UPDATE_KEY_USES uses");
    throwOnSQLiteError(sqlite3_bind_text(STMT_UPDATE_KEY_USES, 2, zPublicKey.c_str(), -1, SQLITE_TRANSIENT), mConn, "bind UPDATE_KEY_USES publickey");
    throwOnSQLiteError(sqlite3_step(STMT_UPDATE_KEY_USES), mConn, "step UPDATE_KEY_USES");
}

void Wallet::updateAllKeyUses(int zUses) {
    throwOnSQLiteError(sqlite3_reset(STMT_UPDATE_ALL_KEY_USES), mConn, "reset UPDATE_ALL_KEY_USES");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_UPDATE_ALL_KEY_USES), mConn, "clear UPDATE_ALL_KEY_USES");
    throwOnSQLiteError(sqlite3_bind_int(STMT_UPDATE_ALL_KEY_USES, 1, zUses), mConn, "bind UPDATE_ALL_KEY_USES uses");
    throwOnSQLiteError(sqlite3_step(STMT_UPDATE_ALL_KEY_USES), mConn, "step UPDATE_ALL_KEY_USES");
}

void Wallet::updateIncrementAllKeyUses(int zIncrementUses) {
    throwOnSQLiteError(sqlite3_reset(STMT_UPDATE_INC_ALL_KEY_USES), mConn, "reset UPDATE_INC_ALL_KEY_USES");
    throwOnSQLiteError(sqlite3_clear_bindings(STMT_UPDATE_INC_ALL_KEY_USES), mConn, "clear UPDATE_INC_ALL_KEY_USES");
    throwOnSQLiteError(sqlite3_bind_int(STMT_UPDATE_INC_ALL_KEY_USES, 1, zIncrementUses), mConn, "bind UPDATE_INC_ALL_KEY_USES inc");
    throwOnSQLiteError(sqlite3_step(STMT_UPDATE_INC_ALL_KEY_USES), mConn, "step UPDATE_INC_ALL_KEY_USES");
}

void Wallet::backupToFile(const std::filesystem::path& targetFile) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!mConn) {
        throw std::runtime_error("Wallet connection not initialized");
    }
    
    // Ensure the target directory exists
    std::error_code ec;
    std::filesystem::create_directories(targetFile.parent_path(), ec);
    if (ec && ec != std::errc::file_exists) {
        throw std::runtime_error("Failed to create backup directory: " + ec.message());
    }
    
    // Open the destination database
    sqlite3* pBackupDb = nullptr;
    int rc = sqlite3_open(targetFile.string().c_str(), &pBackupDb);
    if (rc != SQLITE_OK) {
        std::string err = pBackupDb ? sqlite3_errmsg(pBackupDb) : "unknown";
        if (pBackupDb) {
            sqlite3_close(pBackupDb);
        }
        throw std::runtime_error("Failed to open backup database: " + err);
    }
    
    // Initialize the backup
    sqlite3_backup* pBackup = sqlite3_backup_init(pBackupDb, "main", mConn, "main");
    if (!pBackup) {
        std::string err = sqlite3_errmsg(pBackupDb);
        sqlite3_close(pBackupDb);
        throw std::runtime_error("Failed to initialize backup: " + err);
    }
    
    // Perform the backup in one step (-1 means copy entire database)
    rc = sqlite3_backup_step(pBackup, -1);
    if (rc != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(pBackupDb);
        sqlite3_backup_finish(pBackup);
        sqlite3_close(pBackupDb);
        throw std::runtime_error("Backup step failed: " + err);
    }
    
    // Finalize the backup
    rc = sqlite3_backup_finish(pBackup);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(pBackupDb);
        sqlite3_close(pBackupDb);
        throw std::runtime_error("Backup finalization failed: " + err);
    }
    
    // Close the backup database
    rc = sqlite3_close(pBackupDb);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to close backup database");
    }
}

} // namespace wallet
} // namespace database
} // namespace minima
} // namespace org