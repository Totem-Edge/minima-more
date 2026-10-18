#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

// Forward declarations for project classes used in members/signatures
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }

namespace org { namespace minima { namespace database { namespace txpowdb { namespace ram {
class RamDB;
class RamData;
} } } } }

namespace org { namespace minima { namespace database { namespace txpowdb { namespace sql {
class TxPoWSqlDB;
} } } } }

namespace org { namespace minima { namespace database { namespace txpowdb { namespace onchain {
class TxPoWOnChainDB;
} } } } }

namespace org {
namespace minima {
namespace database {
namespace txpowdb {

class TxPoWDB {
public:
    TxPoWDB();

    // PIMPL Fix for unique_ptr to incomplete types
    virtual ~TxPoWDB();
    TxPoWDB(TxPoWDB&&) noexcept;
    TxPoWDB& operator=(TxPoWDB&&) noexcept;
    TxPoWDB(const TxPoWDB&) = delete;
    TxPoWDB& operator=(const TxPoWDB&) = delete;

    // Load/Save/Close SQL DBs
    void loadSQLDB(const std::filesystem::path& zFile);
    void hardCloseSQLDB();
    void saveDB(bool zCompact);

    // Add a TxPoW to RAM and SQL. Returns relevance.
    bool addTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW);

    // Add a TxPoW to SQL only if not present
    void addSQLTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW);

    // Find a specific TxPoW (RAM first, then SQL)
    std::shared_ptr<org::minima::objects::TxPoW> getTxPoW(const std::string& zTxPoWID);

    // Resolve list of IDs to TxPoWs (skips missing)
    std::vector<std::shared_ptr<org::minima::objects::TxPoW>>
    getAllTxPoW(const std::vector<std::string>& zTxPoWID);

    // Existence check (RAM then SQL)
    bool exists(const std::string& zTxPoWID);

    // Find children blocks of a given parent
    std::vector<std::shared_ptr<org::minima::objects::TxPoW>> getChildBlocks(const std::string& zParentTxPoWID);

    // Sizes
    int getRamSize();
    int getSqlSize();

    // SQL file path
    std::filesystem::path getSqlFile();

    // Access underlying DBs
    org::minima::database::txpowdb::sql::TxPoWSqlDB* getSQLDB();
    org::minima::database::txpowdb::onchain::TxPoWOnChainDB* getOnChainDB();

    // Cleaning
    void cleanDBRAM();
    void wipeDBRAM();
    void cleanDBSQL();

    // Mempool specific
    void clearMainChainTxns();
    void setOnMainChain(const std::string& zTxPoWID);
    void setInCascade(const std::string& zTxPoWID);
    std::vector<std::shared_ptr<org::minima::objects::TxPoW>> getAllUnusedTxns();
    void removeMemPoolTxPoW(const std::string& zTxPoWID);
    bool checkMempoolCoins(const org::minima::objects::base::MiniData& zCoinID);

    // Full mempool snapshot
    std::unordered_map<std::string, std::shared_ptr<org::minima::database::txpowdb::ram::RamData>> getCompleteMemPool();

private:
    std::unique_ptr<org::minima::database::txpowdb::ram::RamDB>        mRamDB;
    std::unique_ptr<org::minima::database::txpowdb::sql::TxPoWSqlDB>   mSqlDB;
    std::unique_ptr<org::minima::database::txpowdb::onchain::TxPoWOnChainDB> mOnChainDB;
};

} // namespace txpowdb
} // namespace database
} // namespace minima
} // namespace org