#include "org/minima/utils/mysql/my_s_q_l_connect.hpp"

#include <cstring>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

// Build-time flag to control availability of MariaDB/MySQL client.
// Default: 0 (no external dependency required to compile).
#ifndef ORG_MINIMA_HAVE_MARIADB
#define ORG_MINIMA_HAVE_MARIADB 0
#endif

#if ORG_MINIMA_HAVE_MARIADB
// Include one of the common client headers; the build system should ensure correctness.
#include <mysql/mysql.h>
#endif

namespace org {
namespace minima {
namespace utils {
namespace mysql {

// Utility: current time in milliseconds
std::uint64_t MySQLConnect::currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Utility: parse host:port
bool MySQLConnect::parseHostPort(const std::string& hostPort, std::string& hostOut, unsigned int& portOut) {
    auto pos = hostPort.find(':');
    if (pos == std::string::npos) {
        hostOut = hostPort;
        portOut = 3306;
        return true;
    }
    hostOut = hostPort.substr(0, pos);
    std::string portstr = hostPort.substr(pos + 1);
    try {
        int p = std::stoi(portstr);
        if (p <= 0) return false;
        portOut = static_cast<unsigned int>(p);
        return true;
    } catch (...) {
        return false;
    }
}

// Constructors
MySQLConnect::MySQLConnect(const std::string& zHost,
                           const std::string& zDatabase,
                           const std::string& zUsername,
                           const std::string& zPassword)
    : mMySQLHost(zHost)
    , mDatabase(zDatabase)
    , mUsername(zUsername)
    , mPassword(zPassword)
    , mReadOnly(false)
{}

MySQLConnect::MySQLConnect(const std::string& zHost,
                           const std::string& zDatabase,
                           const std::string& zUsername,
                           const std::string& zPassword,
                           bool zReadOnly)
    : mMySQLHost(zHost)
    , mDatabase(zDatabase)
    , mUsername(zUsername)
    , mPassword(zPassword)
    , mReadOnly(zReadOnly)
{}

// Move operations
MySQLConnect::MySQLConnect(MySQLConnect&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    mMySQLHost = std::move(other.mMySQLHost);
    mDatabase = std::move(other.mDatabase);
    mUsername = std::move(other.mUsername);
    mPassword = std::move(other.mPassword);
    mReadOnly = other.mReadOnly;
    mDebug = other.mDebug;

    mConnection = other.mConnection; other.mConnection = nullptr;
    mStmtInsertSyncBlock = other.mStmtInsertSyncBlock; other.mStmtInsertSyncBlock = nullptr;
    mStmtSaveCascade = other.mStmtSaveCascade; other.mStmtSaveCascade = nullptr;
    mStmtInsertCoin = other.mStmtInsertCoin; other.mStmtInsertCoin = nullptr;
    mStmtInsertTxPoW = other.mStmtInsertTxPoW; other.mStmtInsertTxPoW = nullptr;
}

MySQLConnect& MySQLConnect::operator=(MySQLConnect&& other) noexcept {
    if (this != &other) {
        shutdown(); // cleanup current
        std::lock_guard<std::mutex> lock_other(other.m_mutex);
        mMySQLHost = std::move(other.mMySQLHost);
        mDatabase = std::move(other.mDatabase);
        mUsername = std::move(other.mUsername);
        mPassword = std::move(other.mPassword);
        mReadOnly = other.mReadOnly;
        mDebug = other.mDebug;

        mConnection = other.mConnection; other.mConnection = nullptr;
        mStmtInsertSyncBlock = other.mStmtInsertSyncBlock; other.mStmtInsertSyncBlock = nullptr;
        mStmtSaveCascade = other.mStmtSaveCascade; other.mStmtSaveCascade = nullptr;
        mStmtInsertCoin = other.mStmtInsertCoin; other.mStmtInsertCoin = nullptr;
        mStmtInsertTxPoW = other.mStmtInsertTxPoW; other.mStmtInsertTxPoW = nullptr;
    }
    return *this;
}

// Destructor
MySQLConnect::~MySQLConnect() {
    shutdown();
}

// Helper to escape strings
#if ORG_MINIMA_HAVE_MARIADB
static std::string mysql_escape(MYSQL* conn, const std::string& in) {
    std::string out;
    out.resize(in.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, &out[0], in.c_str(), static_cast<unsigned long>(in.size()));
    out.resize(len);
    return out;
}
#endif

// Init connection and schema
void MySQLConnect::init() {
#if ORG_MINIMA_HAVE_MARIADB
    // Initialize connection
    mConnection = mysql_init(nullptr);
    if (!mConnection) {
        throw std::runtime_error("mysql_init failed");
    }

    std::string host;
    unsigned int port = 3306;
    if (!parseHostPort(mMySQLHost, host, port)) {
        mysql_close(reinterpret_cast<MYSQL*>(mConnection));
        mConnection = nullptr;
        throw std::runtime_error("Invalid host:port format for MySQL host");
    }

    if (mDebug) {
        org::minima::utils::MinimaLogger::log("JDBC - mysql://" + mMySQLHost + "/" + mDatabase + "?autoReconnect=true");
        org::minima::utils::MinimaLogger::log("Username:" + mUsername + " Password:" + mPassword);
    }

    if (!mysql_real_connect(reinterpret_cast<MYSQL*>(mConnection),
                            host.c_str(),
                            mUsername.c_str(),
                            mPassword.c_str(),
                            mDatabase.c_str(),
                            port,
                            nullptr,
                            0)) {
        std::string err = mysql_error(reinterpret_cast<MYSQL*>(mConnection));
        mysql_close(reinterpret_cast<MYSQL*>(mConnection));
        mConnection = nullptr;
        throw std::runtime_error("mysql_real_connect failed: " + err);
    }

    // Create tables if not read only
    if (!mReadOnly) {
        auto exec = [&](const std::string& sql) {
            if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql.c_str()) != 0) {
                throw std::runtime_error("SQL exec failed: " + std::string(mysql_error(reinterpret_cast<MYSQL*>(mConnection))) + " SQL: " + sql);
            }
        };

        const std::string create_syncblock =
            "CREATE TABLE IF NOT EXISTS `syncblock` ("
            "  `id` INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "  `txpowid` varchar(80) NOT NULL UNIQUE,"
            "  `block` bigint NOT NULL UNIQUE,"
            "  `timemilli` bigint NOT NULL,"
            "  `syncdata` mediumblob NOT NULL"
            ")";

        const std::string create_cascade =
            "CREATE TABLE IF NOT EXISTS `cascadedata` ("
            "  `id` INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "  `cascadetip` BIGINT NOT NULL,"
            "  `fulldata` mediumblob NOT NULL"
            ")";

        const std::string create_coins =
            "CREATE TABLE IF NOT EXISTS `coins` ("
            "  `id` bigint NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "  `coinid` varchar(128) NOT NULL,"
            "  `address` varchar(128) NOT NULL,"
            "  `amount` varchar(128) NOT NULL,"
            "  `amountdouble` double NOT NULL,"
            "  `tokenid` varchar(128) NOT NULL,"
            "  `storestate` int NOT NULL,"
            "  `state` text,"
            "  `mmrentrynumber` bigint NOT NULL,"
            "  `spent` int NOT NULL,"
            "  `blockcreated` bigint NOT NULL,"
            "  `blockspent` bigint NOT NULL,"
            "  `date` varchar(128) NOT NULL,"
            "  `token` text,"
            "  `tokenamount` double NOT NULL"
            ")";

        const std::string create_txpow =
            "CREATE TABLE IF NOT EXISTS `txpow` ("
            "  `id` bigint NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "  `txpowid` varchar(80) NOT NULL UNIQUE,"
            "  `txpowdata` mediumblob NOT NULL"
            ")";

        exec(create_syncblock);
        exec(create_cascade);
        exec(create_coins);
        exec(create_txpow);
    }

    // Prepare statements we need frequently (INSERTs)
    auto prepare_stmt = [&](const std::string& sql)->MYSQL_STMT* {
        MYSQL_STMT* stmt = mysql_stmt_init(reinterpret_cast<MYSQL*>(mConnection));
        if (!stmt) {
            throw std::runtime_error("mysql_stmt_init failed");
        }
        if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
            std::string err = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            throw std::runtime_error("mysql_stmt_prepare failed: " + err + " SQL: " + sql);
        }
        return stmt;
    };

    mStmtInsertSyncBlock = reinterpret_cast<st_mysql_stmt*>(
        prepare_stmt("INSERT IGNORE INTO syncblock ( txpowid, block, timemilli, syncdata ) VALUES ( ?, ?, ?, ? )")
    );
    mStmtSaveCascade     = reinterpret_cast<st_mysql_stmt*>(
        prepare_stmt("INSERT INTO cascadedata ( cascadetip, fulldata ) VALUES ( ?, ? )")
    );
    mStmtInsertCoin      = reinterpret_cast<st_mysql_stmt*>(
        prepare_stmt("INSERT INTO coins(coinid,address,amount,amountdouble,tokenid,storestate,state,mmrentrynumber,spent,blockcreated,blockspent,date,token,tokenamount) VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )")
    );
    mStmtInsertTxPoW     = reinterpret_cast<st_mysql_stmt*>(
        prepare_stmt("INSERT IGNORE INTO txpow ( txpowid, txpowdata ) VALUES ( ?, ? )")
    );
#else
    // No MariaDB/MySQL headers available - compile but fail fast at runtime.
    throw std::runtime_error("MariaDB/MySQL client not enabled at compile time. Define ORG_MINIMA_HAVE_MARIADB=1 and link against libmariadb.");
#endif
}

void MySQLConnect::shutdown() {
#if ORG_MINIMA_HAVE_MARIADB
    // Close statements
    if (mStmtInsertSyncBlock) { mysql_stmt_close(reinterpret_cast<MYSQL_STMT*>(mStmtInsertSyncBlock)); mStmtInsertSyncBlock = nullptr; }
    if (mStmtSaveCascade)     { mysql_stmt_close(reinterpret_cast<MYSQL_STMT*>(mStmtSaveCascade));     mStmtSaveCascade     = nullptr; }
    if (mStmtInsertCoin)      { mysql_stmt_close(reinterpret_cast<MYSQL_STMT*>(mStmtInsertCoin));      mStmtInsertCoin      = nullptr; }
    if (mStmtInsertTxPoW)     { mysql_stmt_close(reinterpret_cast<MYSQL_STMT*>(mStmtInsertTxPoW));     mStmtInsertTxPoW     = nullptr; }

    // Close connection
    if (mConnection) {
        mysql_close(reinterpret_cast<MYSQL*>(mConnection));
        mConnection = nullptr;
    }
#else
    // Nothing to do in fallback
#endif
}

void MySQLConnect::wipeAll() {
#if ORG_MINIMA_HAVE_MARIADB
    if (!mConnection) throw std::runtime_error("No MySQL connection");
    const char* drops[] = { "DROP TABLE syncblock", "DROP TABLE cascadedata", "DROP TABLE coins", "DROP TABLE txpow" };
    for (const char* sql : drops) {
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql) != 0) {
            throw std::runtime_error("SQL exec failed: " + std::string(mysql_error(reinterpret_cast<MYSQL*>(mConnection))) + " SQL: " + std::string(sql));
        }
    }
    shutdown();
    init();
#else
    throw std::runtime_error("wipeAll unavailable: MariaDB/MySQL client not present");
#endif
}

void MySQLConnect::wipeCoinsDB() {
#if ORG_MINIMA_HAVE_MARIADB
    if (!mConnection) throw std::runtime_error("No MySQL connection");
    const char* dropcoins = "DROP TABLE coins";
    if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), dropcoins) != 0) {
        throw std::runtime_error("SQL exec failed: " + std::string(mysql_error(reinterpret_cast<MYSQL*>(mConnection))) + " SQL: " + std::string(dropcoins));
    }
    shutdown();
    init();
#else
    throw std::runtime_error("wipeCoinsDB unavailable: MariaDB/MySQL client not present");
#endif
}

// Prepared statement executors

bool MySQLConnect::execPreparedInsertSyncBlock(const std::string& txpowid,
                                               long long block,
                                               long long timemilli,
                                               const std::vector<std::uint8_t>& syncdata) {
#if ORG_MINIMA_HAVE_MARIADB
    if (!mStmtInsertSyncBlock) return false;

    MYSQL_BIND bind[4];
    std::memset(bind, 0, sizeof(bind));

    // 1: txpowid (string)
    unsigned long txpowid_len = static_cast<unsigned long>(txpowid.size());
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(txpowid.c_str());
    bind[0].buffer_length = txpowid_len;
    bind[0].length = &txpowid_len;

    // 2: block (long long)
    bind[1].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[1].buffer = &block;
    bind[1].is_unsigned = 0;

    // 3: timemilli (long long)
    bind[2].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[2].buffer = &timemilli;
    bind[2].is_unsigned = 0;

    // 4: syncdata (blob)
    unsigned long blob_len = static_cast<unsigned long>(syncdata.size());
    bind[3].buffer_type = MYSQL_TYPE_LONG_BLOB;
    bind[3].buffer = const_cast<unsigned char*>(syncdata.data());
    bind[3].buffer_length = blob_len;
    bind[3].length = &blob_len;

    if (mysql_stmt_bind_param(reinterpret_cast<MYSQL_STMT*>(mStmtInsertSyncBlock), bind) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("InsertSyncBlock bind failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtInsertSyncBlock)));
        return false;
    }

    if (mysql_stmt_execute(reinterpret_cast<MYSQL_STMT*>(mStmtInsertSyncBlock)) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("InsertSyncBlock execute failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtInsertSyncBlock)));
        return false;
    }
    return true;
#else
    (void)txpowid; (void)block; (void)timemilli; (void)syncdata;
    return false;
#endif
}

bool MySQLConnect::execPreparedSaveCascade(long long cascadetip,
                                           const std::vector<std::uint8_t>& fulldata) {
#if ORG_MINIMA_HAVE_MARIADB
    if (!mStmtSaveCascade) return false;

    MYSQL_BIND bind[2];
    std::memset(bind, 0, sizeof(bind));

    // 1: cascadetip (long long)
    bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[0].buffer = &cascadetip;
    bind[0].is_unsigned = 0;

    // 2: fulldata (blob)
    unsigned long blob_len = static_cast<unsigned long>(fulldata.size());
    bind[1].buffer_type = MYSQL_TYPE_LONG_BLOB;
    bind[1].buffer = const_cast<unsigned char*>(fulldata.data());
    bind[1].buffer_length = blob_len;
    bind[1].length = &blob_len;

    if (mysql_stmt_bind_param(reinterpret_cast<MYSQL_STMT*>(mStmtSaveCascade), bind) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("SaveCascade bind failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtSaveCascade)));
        return false;
    }

    if (mysql_stmt_execute(reinterpret_cast<MYSQL_STMT*>(mStmtSaveCascade)) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("SaveCascade execute failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtSaveCascade)));
        return false;
    }
    return true;
#else
    (void)cascadetip; (void)fulldata;
    return false;
#endif
}

bool MySQLConnect::execPreparedInsertCoin(const std::vector<std::string>& stringFields,
                                          const std::vector<double>& doubleFields,
                                          const std::vector<long long>& longFields,
                                          const std::vector<int>& intFields) {
#if ORG_MINIMA_HAVE_MARIADB
    // Expected sizes: stringFields 7, doubleFields 2, longFields 3, intFields 2
    if (!mStmtInsertCoin) return false;
    if (stringFields.size() != 7 || doubleFields.size() != 2 || longFields.size() != 3 || intFields.size() != 2) {
        org::minima::utils::MinimaLogger::log("InsertCoin param vector sizes invalid");
        return false;
    }

    MYSQL_BIND bind[14];
    std::memset(bind, 0, sizeof(bind));

    // Track lengths for strings
    unsigned long slen[7];
    for (size_t i = 0; i < 7; ++i) slen[i] = static_cast<unsigned long>(stringFields[i].size());

    // 1 coinid (string)
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(stringFields[0].c_str());
    bind[0].buffer_length = slen[0];
    bind[0].length = &slen[0];

    // 2 address (string)
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = const_cast<char*>(stringFields[1].c_str());
    bind[1].buffer_length = slen[1];
    bind[1].length = &slen[1];

    // 3 amount (string)
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = const_cast<char*>(stringFields[2].c_str());
    bind[2].buffer_length = slen[2];
    bind[2].length = &slen[2];

    // 4 amountdouble (double)
    bind[3].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[3].buffer = const_cast<double*>(&doubleFields[0]);

    // 5 tokenid (string)
    bind[4].buffer_type = MYSQL_TYPE_STRING;
    bind[4].buffer = const_cast<char*>(stringFields[3].c_str());
    bind[4].buffer_length = slen[3];
    bind[4].length = &slen[3];

    // 6 storestate (int)
    bind[5].buffer_type = MYSQL_TYPE_LONG;
    bind[5].buffer = const_cast<int*>(&intFields[0]);
    bind[5].is_unsigned = 0;

    // 7 state (string)
    bind[6].buffer_type = MYSQL_TYPE_STRING;
    bind[6].buffer = const_cast<char*>(stringFields[4].c_str());
    bind[6].buffer_length = slen[4];
    bind[6].length = &slen[4];

    // 8 mmrentrynumber (long long)
    bind[7].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[7].buffer = const_cast<long long*>(&longFields[0]);
    bind[7].is_unsigned = 0;

    // 9 spent (int)
    bind[8].buffer_type = MYSQL_TYPE_LONG;
    bind[8].buffer = const_cast<int*>(&intFields[1]);
    bind[8].is_unsigned = 0;

    // 10 blockcreated (long long)
    bind[9].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[9].buffer = const_cast<long long*>(&longFields[1]);
    bind[9].is_unsigned = 0;

    // 11 blockspent (long long)
    bind[10].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[10].buffer = const_cast<long long*>(&longFields[2]);
    bind[10].is_unsigned = 0;

    // 12 date (string)
    bind[11].buffer_type = MYSQL_TYPE_STRING;
    bind[11].buffer = const_cast<char*>(stringFields[5].c_str());
    bind[11].buffer_length = slen[5];
    bind[11].length = &slen[5];

    // 13 token (string)
    bind[12].buffer_type = MYSQL_TYPE_STRING;
    bind[12].buffer = const_cast<char*>(stringFields[6].c_str());
    bind[12].buffer_length = slen[6];
    bind[12].length = &slen[6];

    // 14 tokenamount (double)
    bind[13].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[13].buffer = const_cast<double*>(&doubleFields[1]);

    if (mysql_stmt_bind_param(reinterpret_cast<MYSQL_STMT*>(mStmtInsertCoin), bind) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("InsertCoin bind failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtInsertCoin)));
        return false;
    }
    if (mysql_stmt_execute(reinterpret_cast<MYSQL_STMT*>(mStmtInsertCoin)) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("InsertCoin execute failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtInsertCoin)));
        return false;
    }
    return true;
#else
    (void)stringFields; (void)doubleFields; (void)longFields; (void)intFields;
    return false;
#endif
}

bool MySQLConnect::execPreparedInsertTxPoW(const std::string& txpowid,
                                           const std::vector<std::uint8_t>& txpowdata) {
#if ORG_MINIMA_HAVE_MARIADB
    if (!mStmtInsertTxPoW) return false;

    MYSQL_BIND bind[2];
    std::memset(bind, 0, sizeof(bind));

    unsigned long idlen = static_cast<unsigned long>(txpowid.size());
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(txpowid.c_str());
    bind[0].buffer_length = idlen;
    bind[0].length = &idlen;

    unsigned long blob_len = static_cast<unsigned long>(txpowdata.size());
    bind[1].buffer_type = MYSQL_TYPE_LONG_BLOB;
    bind[1].buffer = const_cast<unsigned char*>(txpowdata.data());
    bind[1].buffer_length = blob_len;
    bind[1].length = &blob_len;

    if (mysql_stmt_bind_param(reinterpret_cast<MYSQL_STMT*>(mStmtInsertTxPoW), bind) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("InsertTxPoW bind failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtInsertTxPoW)));
        return false;
    }
    if (mysql_stmt_execute(reinterpret_cast<MYSQL_STMT*>(mStmtInsertTxPoW)) != 0) {
        org::minima::utils::MinimaLogger::log(std::string("InsertTxPoW execute failed: ") + mysql_stmt_error(reinterpret_cast<MYSQL_STMT*>(mStmtInsertTxPoW)));
        return false;
    }
    return true;
#else
    (void)txpowid; (void)txpowdata;
    return false;
#endif
}

// getCount
int MySQLConnect::getCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return -1;
        const char* sql = "SELECT Count(*) as tot FROM syncblock";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("getCount query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return -1;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return -1;
        MYSQL_ROW row = mysql_fetch_row(res);
        int total = -1;
        if (row && row[0]) {
            total = std::stoi(row[0]);
        }
        mysql_free_result(res);
        return total;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return -1;
    }
#else
    return -1;
#endif
}

// saveBlock
bool MySQLConnect::saveBlock(const org::minima::objects::TxBlock& zBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return false;

        auto md = org::minima::objects::base::MiniData::getMiniDataVersion(const_cast<org::minima::objects::TxBlock&>(zBlock));
        if (!md) return false;

        std::string txpowid = zBlock.getTxPoW().getTxPoWID();
        long long blocknum = zBlock.getTxPoW().getBlockNumber().getAsLong();
        long long timemilli = static_cast<long long>(currentTimeMillis());

        return execPreparedInsertSyncBlock(txpowid, blocknum, timemilli, md->getBytes());
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return false;
    }
#else
    (void)zBlock;
    return false;
#endif
}

// loadBlockFromID
std::unique_ptr<org::minima::objects::TxBlock> MySQLConnect::loadBlockFromID(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return nullptr;
        std::string esc = mysql_escape(reinterpret_cast<MYSQL*>(mConnection), zTxPoWID);
        std::string sql = "SELECT syncdata FROM syncblock WHERE txpowid='" + esc + "'";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql.c_str()) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("loadBlockFromID query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return nullptr;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return nullptr;
        MYSQL_ROW row = mysql_fetch_row(res);
        std::unique_ptr<org::minima::objects::TxBlock> ret;
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(res);
            if (lengths && row[0]) {
                std::vector<std::uint8_t> blob(lengths[0]);
                std::memcpy(blob.data(), row[0], lengths[0]);
                org::minima::objects::base::MiniData minisync(blob);
                ret = org::minima::objects::TxBlock::convertMiniDataVersion(minisync);
            }
        }
        mysql_free_result(res);
        return ret;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return nullptr;
    }
#else
    (void)zTxPoWID;
    return nullptr;
#endif
}

// loadBlockFromNum
std::unique_ptr<org::minima::objects::TxBlock> MySQLConnect::loadBlockFromNum(long long zBlocknumber) {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return nullptr;
        std::ostringstream os;
        os << "SELECT syncdata FROM syncblock WHERE block=" << zBlocknumber;
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), os.str().c_str()) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("loadBlockFromNum query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return nullptr;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return nullptr;
        MYSQL_ROW row = mysql_fetch_row(res);
        std::unique_ptr<org::minima::objects::TxBlock> ret;
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(res);
            if (lengths && row[0]) {
                std::vector<std::uint8_t> blob(lengths[0]);
                std::memcpy(blob.data(), row[0], lengths[0]);
                org::minima::objects::base::MiniData minisync(blob);
                ret = org::minima::objects::TxBlock::convertMiniDataVersion(minisync);
            }
        }
        mysql_free_result(res);
        return ret;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return nullptr;
    }
#else
    (void)zBlocknumber;
    return nullptr;
#endif
}

// loadFirstBlock
long long MySQLConnect::loadFirstBlock() {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return -1;
        // Java: SELECT block FROM syncblock ORDER BY block DESC LIMIT 1
        const char* sql = "SELECT block FROM syncblock ORDER BY block DESC LIMIT 1";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("loadFirstBlock query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return -1;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return -1;
        MYSQL_ROW row = mysql_fetch_row(res);
        long long block = -1;
        if (row && row[0]) {
            block = std::stoll(row[0]);
        }
        mysql_free_result(res);
        return block;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return -1;
    }
#else
    return -1;
#endif
}

// loadLastBlock
long long MySQLConnect::loadLastBlock() {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return -1;
        // Java: SELECT block FROM syncblock ORDER BY block ASC LIMIT 1
        const char* sql = "SELECT block FROM syncblock ORDER BY block ASC LIMIT 1";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("loadLastBlock query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return -1;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return -1;
        MYSQL_ROW row = mysql_fetch_row(res);
        long long block = -1;
        if (row && row[0]) {
            block = std::stoll(row[0]);
        }
        mysql_free_result(res);
        return block;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return -1;
    }
#else
    return -1;
#endif
}

// loadBlockRange
std::vector<std::unique_ptr<org::minima::objects::TxBlock>>
MySQLConnect::loadBlockRange(const org::minima::objects::base::MiniNumber& zStartBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::unique_ptr<org::minima::objects::TxBlock>> blocks;
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return blocks;
        std::ostringstream os;
        os << "SELECT syncdata FROM syncblock WHERE block>=" << zStartBlock.getAsLong()
           << " ORDER BY block ASC LIMIT " << MAX_SYNCBLOCKS;
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), os.str().c_str()) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("loadBlockRange query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return blocks;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return blocks;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            unsigned long* lengths = mysql_fetch_lengths(res);
            if (lengths && row[0]) {
                std::vector<std::uint8_t> blob(lengths[0]);
                std::memcpy(blob.data(), row[0], lengths[0]);
                org::minima::objects::base::MiniData minisync(blob);
                auto sb = org::minima::objects::TxBlock::convertMiniDataVersion(minisync);
                if (sb) blocks.emplace_back(std::move(sb));
            }
        }
        mysql_free_result(res);
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
    }
#else
    (void)zStartBlock;
#endif
    return blocks;
}

// Cascade
bool MySQLConnect::saveCascade(const org::minima::database::cascade::Cascade& zCascade) {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return false;

        // Serialize Cascade
        auto cascdata_ptr = org::minima::objects::base::MiniData::getMiniDataVersion(
            const_cast<org::minima::database::cascade::Cascade&>(zCascade));
        if (!cascdata_ptr) return false;

        // Note: CascadeNode definition is not available here; cannot access zCascade.getTip()->getTxPoW()...
        long long tipblock = 0;
        org::minima::utils::MinimaLogger::log("saveCascade: CascadeNode not available; storing cascadetip=0");

        return execPreparedSaveCascade(tipblock, cascdata_ptr->getBytes());
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return false;
    }
#else
    (void)zCascade;
    return false;
#endif
}

std::unique_ptr<org::minima::database::cascade::Cascade> MySQLConnect::loadCascade() {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return nullptr;
        const char* sql = "SELECT fulldata FROM cascadedata ORDER BY cascadetip ASC LIMIT 1";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("loadCascade query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return nullptr;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return nullptr;
        MYSQL_ROW row = mysql_fetch_row(res);
        std::unique_ptr<org::minima::database::cascade::Cascade> ret;
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(res);
            if (lengths && row[0]) {
                std::vector<std::uint8_t> blob(lengths[0]);
                std::memcpy(blob.data(), row[0], lengths[0]);
                org::minima::objects::base::MiniData minisync(blob);
                ret = org::minima::database::cascade::Cascade::convertMiniDataVersion(minisync);
            }
        }
        mysql_free_result(res);
        return ret;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return nullptr;
    }
#else
    return nullptr;
#endif
}

// insertCoin
void MySQLConnect::insertCoin(const org::minima::objects::Coin& zCoin, long long zBlockSpent, const std::string& zDate) {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return;

        std::vector<std::string> sfields(7);
        sfields[0] = zCoin.getCoinID().to0xString();
        sfields[1] = zCoin.getAddress().to0xString();
        sfields[2] = zCoin.getAmount().toString();
        double amountdouble = zCoin.getAmount().getAsDouble();
        sfields[3] = zCoin.getTokenID().to0xString();

        int storestate = zCoin.storeState() ? 1 : 0;
        sfields[4] = zCoin.getStateAsJSON().toString();

        // MMREntryNumber API not present in provided headers; set 0
        long long mmrentry = 0;
        org::minima::utils::MinimaLogger::log("insertCoin: MMREntryNumber not accessible; storing mmrentrynumber=0");

        int spent = zCoin.getSpent() ? 1 : 0;
        long long blockcreated = zCoin.getBlockCreated().getAsLong();
        long long blockspent = zBlockSpent;

        sfields[5] = zDate;

        // Token JSON if present
        if (zCoin.getToken() == nullptr) {
            sfields[6] = "";
        } else {
            sfields[6] = zCoin.getToken()->toJSON().toString();
        }

        double tokenamount = zCoin.getTokenAmount().getAsDouble();

        std::vector<double> dfields = { amountdouble, tokenamount };
        std::vector<long long> lfields = { mmrentry, blockcreated, blockspent };
        std::vector<int> ifields = { storestate, spent };

        execPreparedInsertCoin(sfields, dfields, lfields, ifields);
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
    }
#else
    (void)zCoin; (void)zBlockSpent; (void)zDate;
#endif
}

// getMaxCoinBlock
long long MySQLConnect::getMaxCoinBlock() {
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return -1;
        const char* sql = "SELECT MAX(blockcreated) as maxblock from coins";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("getMaxCoinBlock query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return -1;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return -1;
        MYSQL_ROW row = mysql_fetch_row(res);
        long long maxblock = -1;
        if (row && row[0]) {
            maxblock = std::stoll(row[0]);
        }
        mysql_free_result(res);
        return maxblock;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return -1;
    }
#else
    return -1;
#endif
}

// getTotalCoins
long long MySQLConnect::getTotalCoins() {
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return 0;
        const char* sql = "SELECT COUNT(*) as tot from coins";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("getTotalCoins query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return 0;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return 0;
        MYSQL_ROW row = mysql_fetch_row(res);
        long long tot = 0;
        if (row && row[0]) {
            tot = std::stoll(row[0]);
        }
        mysql_free_result(res);
        return tot;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return 0;
    }
#else
    return 0;
#endif
}

// searchCoins
org::minima::utils::json::JSONObject MySQLConnect::searchCoins(const std::string& zQuery, bool zHideToken) {
    std::lock_guard<std::mutex> lock(m_mutex);
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::json::JSONArray;

    JSONObject error;
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) {
            error.put("status", false);
            error.put("results", false);
            error.put("error", true);
            error.put("sql", zQuery);
            return error;
        }

        JSONObject results;
        results.put("sql", zQuery);
        error.put("sql", zQuery);

        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), zQuery.c_str()) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("searchCoins query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            error.put("status", false);
            error.put("results", false);
            error.put("error", true);
            return error;
        }

        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (res) {
            // There are results (SELECT)
            JSONArray allrows;
            int counter = 0;

            unsigned int columnnum = mysql_num_fields(res);

            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                ++counter;
                JSONObject jrow;
                unsigned long* lengths = mysql_fetch_lengths(res);
                for (unsigned int i = 0; i < columnnum; ++i) {
                    MYSQL_FIELD* field = mysql_fetch_field_direct(res, i);
                    std::string column = field ? field->name : ("col" + std::to_string(i+1));
                    if (zHideToken && column == "token") {
                        jrow.put(column, std::string("_hidden_"));
                        continue;
                    }

                    if (!row[i]) {
                        // NULL -> omit
                        continue;
                    }

                    // Type handling similar to Java
                    if (field && (field->type == MYSQL_TYPE_FLOAT || field->type == MYSQL_TYPE_DOUBLE)) {
                        // Format double with 6 decimals
                        try {
                            double dd = std::stod(std::string(row[i], lengths[i]));
                            std::ostringstream os;
                            os.setf(std::ios::fixed);
                            os << std::setprecision(6) << dd;
                            jrow.put(column, os.str());
                        } catch (...) {
                            jrow.put(column, std::string(row[i], lengths[i]));
                        }
                    } else if (field && (field->type == MYSQL_TYPE_BLOB || field->type == MYSQL_TYPE_TINY_BLOB ||
                                         field->type == MYSQL_TYPE_MEDIUM_BLOB || field->type == MYSQL_TYPE_LONG_BLOB)) {
                        // Treat as text if possible
                        jrow.put(column, std::string(row[i], lengths[i]));
                    } else {
                        // Default to string
                        jrow.put(column, std::string(row[i], lengths[i]));
                    }
                }
                allrows.add(jrow);
            }

            results.put("status", true);
            results.put("results", true);
            results.put("count", counter);
            results.put("rows", allrows);

            mysql_free_result(res);
            return results;
        } else {
            // Non-SELECT (update/insert/delete)
            results.put("status", true);
            results.put("results", false);
            return results;
        }
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        error.put("status", false);
        error.put("results", false);
        error.put("error", true);
        return error;
    }
#else
    (void)zHideToken;
    error.put("status", false);
    error.put("results", false);
    error.put("error", true);
    error.put("sql", zQuery);
    return error;
#endif
}

// saveTxPoW
bool MySQLConnect::saveTxPoW(const org::minima::objects::TxPoW& zTxPoW) {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return false;

        auto md = org::minima::objects::base::MiniData::getMiniDataVersion(const_cast<org::minima::objects::TxPoW&>(zTxPoW));
        if (!md) return false;

        std::string txpowid = zTxPoW.getTxPoWID();
        return execPreparedInsertTxPoW(txpowid, md->getBytes());
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return false;
    }
#else
    (void)zTxPoW;
    return false;
#endif
}

// getTxPoW
std::unique_ptr<org::minima::objects::TxPoW> MySQLConnect::getTxPoW(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(m_mutex);
#if ORG_MINIMA_HAVE_MARIADB
    try {
        if (!mConnection) return nullptr;
        std::string esc = mysql_escape(reinterpret_cast<MYSQL*>(mConnection), zTxPoWID);
        std::string sql = "SELECT txpowdata FROM txpow WHERE txpowid='" + esc + "'";
        if (mysql_query(reinterpret_cast<MYSQL*>(mConnection), sql.c_str()) != 0) {
            org::minima::utils::MinimaLogger::log(std::string("getTxPoW query failed: ") + mysql_error(reinterpret_cast<MYSQL*>(mConnection)));
            return nullptr;
        }
        MYSQL_RES* res = mysql_store_result(reinterpret_cast<MYSQL*>(mConnection));
        if (!res) return nullptr;
        MYSQL_ROW row = mysql_fetch_row(res);
        std::unique_ptr<org::minima::objects::TxPoW> ret;
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(res);
            if (lengths && row[0]) {
                std::vector<std::uint8_t> blob(lengths[0]);
                std::memcpy(blob.data(), row[0], lengths[0]);
                org::minima::objects::base::MiniData minisync(blob);
                ret = org::minima::objects::TxPoW::convertMiniDataVersion(minisync);
            }
        }
        mysql_free_result(res);
        return ret;
    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        return nullptr;
    }
#else
    (void)zTxPoWID;
    return nullptr;
#endif
}

} // namespace mysql
} // namespace utils
} // namespace minima
} // namespace org