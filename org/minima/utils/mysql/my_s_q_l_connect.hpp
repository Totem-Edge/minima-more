#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <cstdint>

// Opaque forward declarations for C client types to avoid including external headers in .hpp
struct st_mysql;
struct st_mysql_stmt;

// Namespaced forward declarations for project types (Pitfall 10)
namespace org { namespace minima { namespace database { namespace cascade { class Cascade; } } } }
namespace org { namespace minima { namespace objects { class Coin; } } }
namespace org { namespace minima { namespace objects { class TxBlock; } } }
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

// We return JSONObject by value; include its header
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace utils {
namespace mysql {

class MySQLConnect {
public:
    static constexpr int MAX_SYNCBLOCKS = 250;

    // Constructors
    MySQLConnect(const std::string& zHost,
                 const std::string& zDatabase,
                 const std::string& zUsername,
                 const std::string& zPassword);

    MySQLConnect(const std::string& zHost,
                 const std::string& zDatabase,
                 const std::string& zUsername,
                 const std::string& zPassword,
                 bool zReadOnly);

    // Non-copyable
    MySQLConnect(const MySQLConnect&) = delete;
    MySQLConnect& operator=(const MySQLConnect&) = delete;

    // Movable
    MySQLConnect(MySQLConnect&&) noexcept;
    MySQLConnect& operator=(MySQLConnect&&) noexcept;

    // Destructor
    ~MySQLConnect();

    // Connect / disconnect
    void init();      // throws std::runtime_error when DB client unavailable or on connection errors
    void shutdown();  // no-throw

    // Maintenance
    void wipeAll();       // throws std::runtime_error
    void wipeCoinsDB();   // throws std::runtime_error

    // Cascade
    bool saveCascade(const org::minima::database::cascade::Cascade& zCascade);
    std::unique_ptr<org::minima::database::cascade::Cascade> loadCascade();

    // Sync blocks
    int getCount(); // synchronized; returns -1 on error

    bool saveBlock(const org::minima::objects::TxBlock& zBlock);
    std::unique_ptr<org::minima::objects::TxBlock> loadBlockFromID(const std::string& zTxPoWID);
    std::unique_ptr<org::minima::objects::TxBlock> loadBlockFromNum(long long zBlocknumber);
    long long loadFirstBlock();
    long long loadLastBlock();
    std::vector<std::unique_ptr<org::minima::objects::TxBlock>>
    loadBlockRange(const org::minima::objects::base::MiniNumber& zStartBlock);

    // Coins
    void insertCoin(const org::minima::objects::Coin& zCoin, long long zBlockSpent, const std::string& zDate);
    long long getMaxCoinBlock();
    long long getTotalCoins();

    // Free-form coin search
    org::minima::utils::json::JSONObject searchCoins(const std::string& zQuery, bool zHideToken);

    // TxPoW
    bool saveTxPoW(const org::minima::objects::TxPoW& zTxPoW);
    std::unique_ptr<org::minima::objects::TxPoW> getTxPoW(const std::string& zTxPoWID);

    // Debug toggle
    void setDebug(bool zDebug) { mDebug = zDebug; }
    bool isDebug() const { return mDebug; }

private:
    // Helpers
    static std::uint64_t currentTimeMillis();
    static bool parseHostPort(const std::string& hostPort, std::string& hostOut, unsigned int& portOut);

    // Execution helpers (internal, no locking)
    bool execPreparedInsertSyncBlock(const std::string& txpowid,
                                     long long block,
                                     long long timemilli,
                                     const std::vector<std::uint8_t>& syncdata);

    bool execPreparedSaveCascade(long long cascadetip,
                                 const std::vector<std::uint8_t>& fulldata);

    bool execPreparedInsertCoin(const std::vector<std::string>& stringFields, // 1,2,3,5,7,12,13
                                const std::vector<double>& doubleFields,      // 4,14
                                const std::vector<long long>& longFields,     // 8,10,11
                                const std::vector<int>& intFields);           // 6,9

    bool execPreparedInsertTxPoW(const std::string& txpowid,
                                 const std::vector<std::uint8_t>& txpowdata);

    // Member state
    std::string mMySQLHost;
    std::string mDatabase;
    std::string mUsername;
    std::string mPassword;

    bool mReadOnly {false};
    bool mDebug {false};

    // MariaDB connection and prepared statements (opaque)
    st_mysql* mConnection {nullptr};

    st_mysql_stmt* mStmtInsertSyncBlock {nullptr};
    st_mysql_stmt* mStmtSaveCascade {nullptr};
    st_mysql_stmt* mStmtInsertCoin {nullptr};
    st_mysql_stmt* mStmtInsertTxPoW {nullptr};

    // Mutex for synchronized methods
    std::mutex m_mutex;
};

} // namespace mysql
} // namespace utils
} // namespace minima
} // namespace org