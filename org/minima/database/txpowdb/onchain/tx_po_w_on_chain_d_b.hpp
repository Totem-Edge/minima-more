#pragma once

#include "org/minima/utils/sql_d_b.hpp"

#include <string>
#include <mutex>

// Forward declarations to avoid heavy includes in header
struct sqlite3;

namespace org { namespace minima { namespace objects { namespace base {
class MiniNumber;
} } } }

namespace org { namespace minima { namespace utils { namespace json {
class JSONObject;
class JSONArray;
} } } }

namespace org {
namespace minima {
namespace database {
namespace txpowdb {
namespace onchain {

class TxPoWOnChainDB : public org::minima::utils::SqlDB {
public:
    // How long does data remain in the SQL DB in milliseconds
    static constexpr long long MAX_ONCHAINSQL_MILLI = 1000LL * 60 * 60 * 24 * 1000;

    TxPoWOnChainDB();
    ~TxPoWOnChainDB() override;

    // Recreate main table (translated from Java wipeDB)
    void wipeDB();

    // Add an on-chain TxPoW record
    bool addOnChainTxPoW(const std::string& zBlockID,
                         const org::minima::objects::base::MiniNumber& zBlock,
                         const std::string& zTxPoWID);

    // Get a specific TxPoW record
    org::minima::utils::json::JSONObject getOnChainTxPoW(const std::string& zTxPoWID);

    // Get all TxPoW entries for a given block ID
    org::minima::utils::json::JSONArray getInBlockTxPoW(const std::string& zBlockID);

    // Get all TxPoW entries for a given block number
    org::minima::utils::json::JSONArray getInBlockTxPoW(long long zBlock);

    // Get the earliest and latest TxPoW entries by timemilli
    org::minima::utils::json::JSONObject getFirstTxPoW();
    org::minima::utils::json::JSONObject getLastTxPoW();

    // Get the total number of rows in onchaintxpow
    int getSize();

    // Returns how many rows were deleted
    int cleanDB();
    int cleanDB(bool zHard);

protected:
    // Perform the Create SQL
    void createSQL() override;

private:
    void ensureLocalOpen();
    static std::string formatDateString(long long millis);

private:
    sqlite3* mLocalConn;
    std::mutex mMutex;
};

} // namespace onchain
} // namespace txpowdb
} // namespace database
} // namespace minima
} // namespace org