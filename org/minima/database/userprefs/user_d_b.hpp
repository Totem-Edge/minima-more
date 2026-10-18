#pragma once

#include <string>
#include <vector>

#include "org/minima/utils/json_d_b.hpp"

// Namespaced forward declarations for project dependencies used in signatures
namespace org { namespace minima { namespace objects { namespace base {
class MiniData;
class MiniNumber;
}}}}

namespace org { namespace minima { namespace utils { namespace json {
class JSONArray;
}}}}

namespace org {
namespace minima {
namespace database {
namespace userprefs {

class UserDB : public org::minima::utils::JsonDB {
public:
    UserDB();
    virtual ~UserDB() = default;

    // Welcome message
    void setWelcome(const std::string& zWelcome);
    std::string getWelcome() const;

    // Incentive Cash User
    std::string getIncentiveCashUserID() const;
    void setIncentiveCashUserID(const std::string& zUID);

    // Web Hooks
    void setWebHooks(const std::vector<std::string>& zWebHooks);
    std::vector<std::string> getWebHooks();

    // Custom Transactions
    org::minima::objects::base::MiniData loadCustomTransactions() const;
    void saveCustomTransactions(const org::minima::objects::base::MiniData& zCompleteDB);

    // Hash rate
    void setHashRate(const org::minima::objects::base::MiniNumber& zHashesPerSec);
    org::minima::objects::base::MiniNumber getHashRate() const;

    // Auto backup
    bool isAutoBackup() const;
    void setAutoBackup(bool zAuto);

    // Magic desired values
    org::minima::objects::base::MiniNumber getMagicDesiredKISSVM() const;
    void setMagicDesiredKISSVM(const org::minima::objects::base::MiniNumber& zKISSVM);
    org::minima::objects::base::MiniNumber getMagicMaxTxPoWSize() const;
    void setMagicMaxTxPoWSize(const org::minima::objects::base::MiniNumber& zMaxSize);
    org::minima::objects::base::MiniNumber getMagicMaxTxns() const;
    void setMagicMaxTxns(const org::minima::objects::base::MiniNumber& zMaxTxns);

    // Encrypted Seed
    void setEncryptedSeed(const org::minima::objects::base::MiniData& zEncryptedSeed);
    org::minima::objects::base::MiniData getEncryptedSeed() const;

    // MySQL auto backup and login details
    bool getAutoLoginDetailsMySQL() const;
    void setAutoLoginDetailsMySQL(bool zLoginDetails);
    bool getAutoBackupMySQL() const;
    void setAutoBackupMySQL(bool zAuto);
    bool getAutoBackupMySQLCoins() const;
    void setAutoBackupMySQLCoins(bool zAuto);

    void setAutoMySQLHost(const std::string& zHost);
    std::string getAutoMySQLHost() const;
    void setAutoMySQLDB(const std::string& zDB);
    std::string getAutoMySQLDB() const;
    void setAutoMySQLUser(const std::string& zUser);
    std::string getAutoMySQLUser() const;
    void setAutoMySQLPassword(const std::string& zPassword);
    std::string getAutoMySQLPassword() const;

    // Slave node properties
    bool isSlaveNode() const;
    std::string getSlaveNodeHost() const;
    void setSlaveNode(bool zEnabled, const std::string& zHost);

    // Default MiniHUB
    std::string getDefaultMiniHUB() const;
    void setDefaultMiniHUB(const std::string& zMiniDAPPID);

    // Uninstalled MiniDAPP
    void clearUninstalledMiniDAPP();
    org::minima::utils::json::JSONArray getUninstalledMiniDAPP();
    void removeUninstalledMiniDAPP(const std::string& zName);
    void addUninstalledMiniDAPP(const std::string& zName);
    bool checkUninstalledMiniDAPP(const std::string& zName);

    // RPC Users
    org::minima::utils::json::JSONArray getRPCUsers();
    void setRPCUsers(const org::minima::utils::json::JSONArray& zNewUsers);
};

} // namespace userprefs
} // namespace database
} // namespace minima
} // namespace org