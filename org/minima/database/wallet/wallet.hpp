#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <cstdint>
#include <filesystem>

// Base class include (Inheritance required)
#include "org/minima/utils/sql_d_b.hpp"
#include "org/minima/database/wallet/seed_row.hpp"
#include "org/minima/database/wallet/key_row.hpp"
#include "org/minima/database/wallet/script_row.hpp"

// Forward declarations for project types used in signatures and implementation
namespace org { namespace minima { namespace objects { namespace base {
class MiniData;
} } } }
namespace org { namespace minima { namespace objects {
class Address;
} } }
namespace org { namespace minima { namespace objects { namespace keys {
class Signature;
class TreeKey;
} } } }
namespace org { namespace minima { namespace system { namespace commands { namespace send {
class multisig;
} } } } }
namespace org { namespace minima { namespace system { namespace params {
class GeneralParams;
} } } }
namespace org { namespace minima { namespace utils {
class BIP39;
class Crypto;
class MinimaLogger;
} } }

// Forward-declare sqlite C types to avoid heavy includes in the header
struct sqlite3;
struct sqlite3_stmt;

namespace org {
namespace minima {
namespace database {
namespace wallet {

// Simple row containers to mirror Java SeedRow, KeyRow, ScriptRow

// class SeedRow {
// public:
//     SeedRow() = default;
//     SeedRow(const std::string& phrase, const std::string& seed)
//         : mPhrase(phrase), mSeed(seed) {}

//     const std::string& getPhrase() const { return mPhrase; }
//     const std::string& getSeed() const { return mSeed; }

// private:
//     std::string mPhrase;
//     std::string mSeed;
// };

// class KeyRow {
// public:
//     KeyRow() = default;
//     KeyRow(int size, int depth, int uses, int maxuses,
//            const std::string& modifier,
//            const std::string& privatekey,
//            const std::string& publickey)
//         : mSize(size), mDepth(depth), mUses(uses), mMaxUses(maxuses),
//           mModifier(modifier), mPrivateKey(privatekey), mPublicKey(publickey) {}

//     int getSize() const { return mSize; }
//     int getDepth() const { return mDepth; }
//     int getUses() const { return mUses; }
//     int getMaxUses() const { return mMaxUses; }
//     const std::string& getModifier() const { return mModifier; }
//     const std::string& getPrivateKey() const { return mPrivateKey; }
//     const std::string& getPublicKey() const { return mPublicKey; }

// private:
//     int mSize = 0;
//     int mDepth = 0;
//     int mUses = 0;
//     int mMaxUses = 0;
//     std::string mModifier;
//     std::string mPrivateKey;
//     std::string mPublicKey;
// };

// class ScriptRow {
// public:
//     ScriptRow() = default;
//     ScriptRow(const std::string& script,
//               const std::string& address,
//               bool simple,
//               bool defaddr,
//               const std::string& publickey,
//               bool track)
//         : mScript(script), mAddress(address), mSimple(simple),
//           mDefault(defaddr), mPublicKey(publickey), mTrack(track) {}

//     const std::string& getScript() const { return mScript; }
//     const std::string& getAddress() const { return mAddress; }
//     bool isSimple() const { return mSimple; }
//     bool isDefault() const { return mDefault; }
//     const std::string& getPublicKey() const { return mPublicKey; }
//     bool isTrack() const { return mTrack; }

// private:
//     std::string mScript;
//     std::string mAddress;
//     bool mSimple = false;
//     bool mDefault = false;
//     std::string mPublicKey;
//     bool mTrack = false;
// };

class Wallet : public org::minima::utils::SqlDB {
public:
    // Static parameter (adjusted for test params on construction)
    static int NUMBER_GETADDRESS_KEYS;

    Wallet();
    virtual ~Wallet();

    // Control flags (non-synchronized accessors, matching Java usage)
    void shuttingDown();

    // Reset DB with a new seed phrase
    void resetDB(const std::string& zNewSeedPhrase);

    // Update seed row
    void updateSeedRow(const std::string& zPhrase, const std::string& zSeed);

    // Synchronized getters/setters for seed
    SeedRow getBaseSeed();
    bool isBaseSeedAvailable();
    void wipeBaseSeedRow();

    // Reset private keys from seed and modifier
    bool resetBaseSeedPrivKeys(const std::string& zPhrase, const std::string& zSeed);

    // Key checks
    bool checkAllPrivateKeys();
    bool checkSingleKey(const std::string& zPrivateKey, const std::string& zModifier);

    // Stop creating new keys
    void setStopNewKeys(bool zStopNewKeys);

    // Default keys
    int getDefaultKeysNumber();
    bool initDefaultKeys(int zMaxNum);
    bool initDefaultKeys(int zMaxNum, bool zLog);
    std::unique_ptr<ScriptRow> getDefaultAddress();

    // Create new objects
    std::unique_ptr<ScriptRow> createNewSimpleAddress(bool zDefault);
    std::unique_ptr<KeyRow> createNewKey();

    // Scripts
    std::unique_ptr<ScriptRow> addScript(const std::string& zScript,
                                         bool zSimple,
                                         bool zDefault,
                                         const std::string& zPublicKey,
                                         bool zTrack);
    void removeScript(const std::string& zAddress);
    std::unique_ptr<ScriptRow> getScriptFromAddress(const std::string& zAddress);

    // Relevance checks
    bool isKeyRelevant(const std::string& zPublicKey);
    bool isAddressRelevant(const std::string& zAddress);
    bool isAddressSimple(const std::string& zAddress);

    // Collections
    std::vector<std::unique_ptr<KeyRow>> getAllKeys();
    std::unique_ptr<KeyRow> getKeyFromPublic(const std::string& zPublicKey);
    std::vector<std::unique_ptr<ScriptRow>> getAllAddresses();

    // Signing
    std::unique_ptr<org::minima::objects::keys::Signature>
    signData(const std::string& zPublicKey, const org::minima::objects::base::MiniData& zData);

    // Update uses
    void updateAllKeyUses(int zUses);
    void updateIncrementAllKeyUses(int zIncrementUses);

    void backupToFile(const std::filesystem::path& targetFile);

protected:
    // Create tables and prepared statements and initialize caches/seed
    void createSQL() override;

private:
    // Internal helpers
    void openSecondaryConnection();  // open mConn if needed
    void closeSecondaryConnection(); // finalize all stmts and close mConn
    void prepareStatements();
    void finalizeStatements();

    void initBaseSeed();
    void updateUsesInternal(int zUses, const std::string& zPublicKey);

    std::vector<std::unique_ptr<ScriptRow>> getAllDefaultAddresses();

    // Build MiniData from a non-negative integer (minimal big-endian encoding)
    static org::minima::objects::base::MiniData miniDataFromUInt64(std::uint64_t v);

private:
    // Secondary SQLite connection dedicated to this class (since SqlDB doesn't expose its handle)
    sqlite3* mConn = nullptr;

    // Prepared statements
    sqlite3_stmt* STMT_CREATE_PUBLIC_KEY = nullptr;
    sqlite3_stmt* STMT_GET_KEY = nullptr;
    sqlite3_stmt* STMT_GET_ALL_KEYS = nullptr;
    sqlite3_stmt* STMT_UPDATE_KEY_USES = nullptr;
    sqlite3_stmt* STMT_UPDATE_ALL_KEY_USES = nullptr;
    sqlite3_stmt* STMT_UPDATE_INC_ALL_KEY_USES = nullptr;

    sqlite3_stmt* STMT_WIPE_PRIVATE_KEYS = nullptr;
    sqlite3_stmt* STMT_UPDATE_PRIVATE_KEYS = nullptr;

    sqlite3_stmt* STMT_ADD_SCRIPT = nullptr;
    sqlite3_stmt* STMT_REMOVE_SCRIPT = nullptr;
    sqlite3_stmt* STMT_LIST_ALL_SCRIPTS = nullptr;
    sqlite3_stmt* STMT_LIST_SIMPLE_SCRIPTS = nullptr;
    sqlite3_stmt* STMT_LIST_TRACK_SCRIPTS = nullptr;
    sqlite3_stmt* STMT_LIST_DEFAULT_SCRIPTS = nullptr;
    sqlite3_stmt* STMT_GET_SCRIPT = nullptr;

    sqlite3_stmt* STMT_SELECT_SEED = nullptr;
    sqlite3_stmt* STMT_INSERT_SEED = nullptr;
    sqlite3_stmt* STMT_UPDATE_SEED = nullptr;

    // Cached lists for O(1) checks
    std::unordered_set<std::string> mAllKeys;
    std::unordered_set<std::string> mAllTrackedAddress;
    std::unordered_set<std::string> mAllSimpleAddress;

    // Base seed
    std::unique_ptr<SeedRow> mBaseSeed;

    // Control flags
    bool mShuttingdown = false;
    bool mStopNewKeys = false;

    // Synchronization for synchronized methods
    std::mutex m_mutex;
};

} // namespace wallet
} // namespace database
} // namespace minima
} // namespace org