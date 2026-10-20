#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "org/minima/utils/sql_d_b.hpp"

// Forward declarations (namespaced per Pitfall 10)
namespace org { namespace minima { namespace database { namespace cascade { class Cascade; } } } }
namespace org { namespace minima { namespace objects { class TxBlock; class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; class MiniData; } } } }
namespace org { namespace minima { namespace system { class Main; } } }

struct sqlite3;       // SQLite C handle forward declaration
struct sqlite3_stmt;  // SQLite prepared statement forward declaration

namespace org {
namespace minima {
namespace database {
namespace archive {

class ArchiveManager : public org::minima::utils::SqlDB {
public:
    ArchiveManager();
    virtual ~ArchiveManager();

    // Non-copyable, non-movable (base likely non-movable due to mutex)
    ArchiveManager(const ArchiveManager&) = delete;
    ArchiveManager& operator=(const ArchiveManager&) = delete;
    ArchiveManager(ArchiveManager&&) noexcept = delete;
    ArchiveManager& operator=(ArchiveManager&&) noexcept = delete;

    // How long data remains in the Archive DB (~200 blocks per day)
    long long MAX_KEEP_BLOCKS;

    // Size of DB (number of rows in syncblock) - returns -1 on error
    int getSize();

    // Ensure cascade is stored if required and consistent with stored blocks
    void checkCascadeRequired(const org::minima::database::cascade::Cascade& zCascade);

    // Load cascade (returns nullptr if not present)
    std::unique_ptr<org::minima::database::cascade::Cascade> loadCascade();

    // Save/load blocks
    bool saveBlock(org::minima::objects::TxBlock& zBlock);

    // Bound the WAL during bulk archive replay.
    void checkpoint();

    // Batch archive inserts inside one transaction (bulk replay).
    void beginSaveBatch();
    void endSaveBatch(bool zCommit = true);

    // Flush any buffered blocks into their compressed chunk (called on
    // save/shutdown so the last <256 blocks are not lost).
    void flushPending();
    std::unique_ptr<org::minima::objects::TxBlock> loadBlock(const std::string& zTxPoWID);

    std::unique_ptr<org::minima::objects::TxBlock> loadFirstBlock();
    std::unique_ptr<org::minima::objects::TxBlock> loadLastBlock();

    std::vector<std::unique_ptr<org::minima::objects::TxBlock>>
    loadSyncBlockRange(const org::minima::objects::base::MiniNumber& zStartBlock);

    org::minima::objects::base::MiniNumber exists(const std::string& zTxPoWID);

    std::vector<std::unique_ptr<org::minima::objects::TxBlock>>
    loadBlockRange(const org::minima::objects::base::MiniNumber& zStartBlock,
                   const org::minima::objects::base::MiniNumber& zEndBlock);

    std::vector<std::unique_ptr<org::minima::objects::TxBlock>>
    loadBlockRange(const org::minima::objects::base::MiniNumber& zStartBlock,
                   const org::minima::objects::base::MiniNumber& zEndBlock,
                   bool zDescending);

    std::unique_ptr<org::minima::objects::TxBlock>
    loadBlockFromNumber(const org::minima::objects::base::MiniNumber& zBlock);

    // Clean DB depending on GeneralParams.ARCHIVE
    int checkForCleanDB();

    // Hard close DB connection handle (our private handle and base)
    void hackShut();

    // Close and reopen the DB (for faster start/shutdown cycles)
    void closeAndReopen();

protected:
    // Create tables and indexes (called by SqlDB::loadDB)
    void createSQL() override;

private:
    // Helpers
    void openConnIfNeeded();
    static std::int64_t nowMillis();

    // Private ops
    bool _intSaveBlock(org::minima::objects::TxBlock& zBlock);
    bool saveCascade(org::minima::database::cascade::Cascade& zCascade);
    int cleanDB();

    // Chunked storage (R1): buffer N blocks, compress the concatenation once.
    void ensureSchemaAndStatements();
    bool flushPendingChunkLocked();
    std::shared_ptr<const std::vector<std::uint8_t>> loadChunkLocked(long long zChunkID);
    bool extractBlockBytes(const void* blob, int blen, long long zChunkID,
                           int zOffset, int zLen,
                           org::minima::objects::base::MiniData& out);

    struct PendingBlock {
        std::string txpowid;
        long long block = 0;
        long long timemilli = 0;
        std::vector<std::uint8_t> data;
    };

private:
    // Our own SQLite connection used for prepared statements and BLOBs
    sqlite3* mConn {nullptr};
    bool mInBatch {false};
    sqlite3_stmt* mStmtInsertRow {nullptr};
    sqlite3_stmt* mStmtInsertChunk {nullptr};
    std::vector<PendingBlock> mPending;
    std::unordered_map<long long, std::shared_ptr<const std::vector<std::uint8_t>>> mChunkCache;

    // Mutex for synchronized methods
    std::mutex m_mutex;
};

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org