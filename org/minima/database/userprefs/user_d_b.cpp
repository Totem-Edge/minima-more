#include "org/minima/database/userprefs/user_d_b.hpp"

#include <algorithm>
#include <cctype>

#include "org/minima/objects/magic.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/mini_util.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace database {
namespace userprefs {

using org::minima::objects::Magic;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::params::GeneralParams;
using org::minima::system::params::GlobalParams;
using org::minima::utils::json::JSONArray;
using org::minima::utils::MiniUtil;

namespace {
// Normalize MiniDAPP names: lowercase and remove spaces, as per Java toLowerCase().replaceAll(" ", "")
std::string normalizeName(const std::string& zName) {
    std::string name = zName;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
    return name;
}
} // anonymous

UserDB::UserDB() : org::minima::utils::JsonDB() {}

// Welcome message
void UserDB::setWelcome(const std::string& zWelcome) {
    setString("welcome", zWelcome);
}

std::string UserDB::getWelcome() const {
    return getString("welcome", std::string("Running Minima ") + GlobalParams::MINIMA_VERSION);
}

// Incentive Cash User
std::string UserDB::getIncentiveCashUserID() const {
    return getString("uid", "");
}

void UserDB::setIncentiveCashUserID(const std::string& zUID) {
    setString("uid", zUID);
}

// Web Hooks
void UserDB::setWebHooks(const std::vector<std::string>& zWebHooks) {
    JSONArray arr;
    for (const auto& hook : zWebHooks) {
        arr.add(hook);
    }
    setJSONArray("webhooks", arr);
}

std::vector<std::string> UserDB::getWebHooks() {
    JSONArray arr = getJSONArray("webhooks");
    return MiniUtil::convertJSONArray(arr);
}

// Custom Transactions
MiniData UserDB::loadCustomTransactions() const {
    return getData("custom_transactions", MiniData::ZERO_TXPOWID());
}

void UserDB::saveCustomTransactions(const MiniData& zCompleteDB) {
    setData("custom_transactions", zCompleteDB);
}

// Hash rate
void UserDB::setHashRate(const MiniNumber& zHashesPerSec) {
    setNumber("hashrate", zHashesPerSec);
}

MiniNumber UserDB::getHashRate() const {
    return getNumber("hashrate", MiniNumber::MILLION());
}

// Auto backup
bool UserDB::isAutoBackup() const {
    return getBoolean("autobackup", false);
}

void UserDB::setAutoBackup(bool zAuto) {
    setBoolean("autobackup", zAuto);
}

// Magic desired values
MiniNumber UserDB::getMagicDesiredKISSVM() const {
    return getNumber("magic_kissvm", Magic::DEFAULT_KISSVM_OPERATIONS);
}

void UserDB::setMagicDesiredKISSVM(const MiniNumber& zKISSVM) {
    setNumber("magic_kissvm", zKISSVM);
}

MiniNumber UserDB::getMagicMaxTxPoWSize() const {
    return getNumber("magic_txpowsize", Magic::DEFAULT_TXPOW_SIZE);
}

void UserDB::setMagicMaxTxPoWSize(const MiniNumber& zMaxSize) {
    setNumber("magic_txpowsize", zMaxSize);
}

MiniNumber UserDB::getMagicMaxTxns() const {
    return getNumber("magic_txns", Magic::DEFAULT_TXPOW_TXNS);
}

void UserDB::setMagicMaxTxns(const MiniNumber& zMaxTxns) {
    setNumber("magic_txns", zMaxTxns);
}

// Encrypted Seed
void UserDB::setEncryptedSeed(const MiniData& zEncryptedSeed) {
    setData("encrypted_seed", zEncryptedSeed);
}

MiniData UserDB::getEncryptedSeed() const {
    return getData("encrypted_seed", MiniData::ZERO_TXPOWID());
}

// MySQL auto backup and login details
bool UserDB::getAutoLoginDetailsMySQL() const {
    return getBoolean("mysql_autologindetails", false);
}

void UserDB::setAutoLoginDetailsMySQL(bool zLoginDetails) {
    setBoolean("mysql_autologindetails", zLoginDetails);
}

bool UserDB::getAutoBackupMySQL() const {
    return getBoolean("mysql_autobackup", false);
}

void UserDB::setAutoBackupMySQL(bool zAuto) {
    setBoolean("mysql_autobackup", zAuto);
}

bool UserDB::getAutoBackupMySQLCoins() const {
    return getBoolean("mysqlcoins_autobackup", false);
}

void UserDB::setAutoBackupMySQLCoins(bool zAuto) {
    setBoolean("mysqlcoins_autobackup", zAuto);
}

void UserDB::setAutoMySQLHost(const std::string& zHost) {
    setString("mysql_host", zHost);
}

std::string UserDB::getAutoMySQLHost() const {
    return getString("mysql_host", "");
}

void UserDB::setAutoMySQLDB(const std::string& zDB) {
    setString("mysql_db", zDB);
}

std::string UserDB::getAutoMySQLDB() const {
    return getString("mysql_db", "");
}

void UserDB::setAutoMySQLUser(const std::string& zUser) {
    setString("mysql_user", zUser);
}

std::string UserDB::getAutoMySQLUser() const {
    return getString("mysql_user", "");
}

void UserDB::setAutoMySQLPassword(const std::string& zPassword) {
    setString("mysql_password", zPassword);
}

std::string UserDB::getAutoMySQLPassword() const {
    return getString("mysql_password", "");
}

// Slave node properties
bool UserDB::isSlaveNode() const {
    return getBoolean("slavenode_enabled", false);
}

std::string UserDB::getSlaveNodeHost() const {
    return getString("slavenode_host", "");
}

void UserDB::setSlaveNode(bool zEnabled, const std::string& zHost) {
    setBoolean("slavenode_enabled", zEnabled);
    setString("slavenode_host", zHost);
}

// Default MiniHUB
std::string UserDB::getDefaultMiniHUB() const {
    return getString("minihub_default", "0x00");
}

void UserDB::setDefaultMiniHUB(const std::string& zMiniDAPPID) {
    setString("minihub_default", zMiniDAPPID);
}

// Uninstalled MiniDAPP
void UserDB::clearUninstalledMiniDAPP() {
    setJSONArray("minidapps_uninstalled", JSONArray());
}

JSONArray UserDB::getUninstalledMiniDAPP() {
    return getJSONArray("minidapps_uninstalled");
}

void UserDB::removeUninstalledMiniDAPP(const std::string& zName) {
    auto all = MiniUtil::convertJSONArray(getUninstalledMiniDAPP());
    std::string name = normalizeName(zName);

    auto it = std::find(all.begin(), all.end(), name);
    if (it != all.end()) {
        all.erase(it); // remove first occurrence (Java's List.remove(Object))
    }

    setJSONArray("minidapps_uninstalled", MiniUtil::convertArrayList(all));
}

void UserDB::addUninstalledMiniDAPP(const std::string& zName) {
    auto all = MiniUtil::convertJSONArray(getUninstalledMiniDAPP());
    std::string name = normalizeName(zName);

    // Remove existing first (as Java does)
    auto it = std::find(all.begin(), all.end(), name);
    if (it != all.end()) {
        all.erase(it);
    }
    // Add
    all.push_back(name);

    setJSONArray("minidapps_uninstalled", MiniUtil::convertArrayList(all));
}

bool UserDB::checkUninstalledMiniDAPP(const std::string& zName) {
    auto all = MiniUtil::convertJSONArray(getUninstalledMiniDAPP());
    std::string name = normalizeName(zName);
    return std::find(all.begin(), all.end(), name) != all.end();
}

// RPC Users
JSONArray UserDB::getRPCUsers() {
    return getJSONArray("rpcusers_allusers");
}

void UserDB::setRPCUsers(const JSONArray& zNewUsers) {
    setJSONArray("rpcusers_allusers", zNewUsers);
}

} // namespace userprefs
} // namespace database
} // namespace minima
} // namespace org