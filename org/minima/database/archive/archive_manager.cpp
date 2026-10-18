#include "org/minima/database/archive/archive_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <sqlite3.h>

#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/mini_file.hpp"

namespace org {
namespace minima {
namespace database {
namespace archive {

using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::database::cascade::Cascade;
using org::minima::utils::MinimaLogger;
using org::minima::system::params::GeneralParams;

ArchiveManager::ArchiveManager()
: SqlDB()
, MAX_KEEP_BLOCKS(2000LL * GeneralParams::NUMBER_DAYS_ARCHIVE)
, mConn(nullptr) {
}

ArchiveManager::~ArchiveManager() {
    if (mConn) {
        sqlite3_close(mConn);
        mConn = nullptr;
    }
}

void ArchiveManager::createSQL() {
    try {
        // Create main table
        const std::string create_syncblock =
            "CREATE TABLE IF NOT EXISTS syncblock ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  txpowid TEXT NOT NULL UNIQUE,"
            "  block INTEGER NOT NULL UNIQUE,"
            "  timemilli INTEGER NOT NULL,"
            "  syncdata BLOB NOT NULL"
            ");";

        // Create cascade table
        const std::string create_cascade =
            "CREATE TABLE IF NOT EXISTS cascadedata ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  cascadetip INTEGER NOT NULL,"
            "  fulldata BLOB NOT NULL"
            ");";

        // Create index
        const std::string create_index =
            "CREATE INDEX IF NOT EXISTS fastsearch ON syncblock ( txpowid, block );";

        // Execute
        executeGenericSQL(create_syncblock);
        executeGenericSQL(create_cascade);
        executeGenericSQL(create_index);
    } catch (...) {
        MinimaLogger::log(std::string("ArchiveManager::createSQL - exception executing DDL"));
    }
}

void ArchiveManager::openConnIfNeeded() {
    if (mConn != nullptr) {
        return;
    }
    // Ensure base DB is open (creates file and calls createSQL)
    checkOpen();

    const std::string path = getSQLFile();
    if (path.empty()) {
        throw std::runtime_error("ArchiveManager: SQL file path is empty.");
    }
    int rc = sqlite3_open(path.c_str(), &mConn);
    if (rc != SQLITE_OK) {
        std::string err = "ArchiveManager: sqlite3_open failed: ";
        err += (mConn ? sqlite3_errmsg(mConn) : "unknown");
        if (mConn) {
            sqlite3_close(mConn);
            mConn = nullptr;
        }
        throw std::runtime_error(err);
    }
    org::minima::utils::SqlDB::applySQLiteTuning(mConn);
}

std::int64_t ArchiveManager::nowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int ArchiveManager::getSize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT COUNT(*) AS tot FROM syncblock;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("getSize prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return -1;
        }

        int ret = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            ret = sqlite3_column_int(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("getSize step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return ret;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return -1;
    }
}

void ArchiveManager::checkCascadeRequired(const Cascade& zCascade) {
    if (zCascade.getLength() <= 0) {
        return;
    }

    try {
        auto casc = loadCascade();
        if (!casc) {
            // If not present, check correctness before saving
            if (!Cascade::checkCascadeCorrect(zCascade)) {
                MinimaLogger::log("[!] INCONSISTENT Cascade to save in Archive.. not saving..");
                return;
            }

            MinimaLogger::log(std::string("Saving Cascade in ARCHIVEDB.. tip : ")
                              + zCascade.getTip()->getTxPoW().getBlockNumber().toString());
            // Need non-const for serialization
            saveCascade(const_cast<Cascade&>(zCascade));
        } else {
            // Check alignment with archive blocks
            auto last = loadLastBlock();
            if (last) {
                MiniNumber archstart = last->getTxPoW().getBlockNumber();
                auto first = loadFirstBlock();
                if (!first) {
                    MinimaLogger::log(std::string("Cascade already in ARCHIVEDB.. tip : ")
                                      + casc->getTip()->getTxPoW().getBlockNumber().toString());
                    return;
                }
                MiniNumber archend = first->getTxPoW().getBlockNumber();

                MiniNumber cascstart = casc->getTip()->getTxPoW().getBlockNumber();
                // cascstart in [archstart - 1, archend]
                if (cascstart.isMoreEqual(archstart.sub(MiniNumber::ONE())) &&
                    cascstart.isLessEqual(archend)) {
                    // OK
                } else {
                    MinimaLogger::log(std::string("RE-Saving Cascade in ARCHIVEDB.. tip : ")
                                      + zCascade.getTip()->getTxPoW().getBlockNumber().toString());
                    saveCascade(const_cast<Cascade&>(zCascade));
                }
            } else {
                MinimaLogger::log(std::string("Cascade already in ARCHIVEDB.. tip : ")
                                  + casc->getTip()->getTxPoW().getBlockNumber().toString());
            }
        }
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
}

bool ArchiveManager::saveCascade(Cascade& zCascade) {
    // Delete old
    try {
        executeGenericSQL("DELETE FROM cascadedata WHERE id>=0;");
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        // Continue anyway
    }

    try {
        // Serialize to MiniData
        std::unique_ptr<MiniData> cascdata = MiniData::getMiniDataVersion(zCascade);
        if (!cascdata) {
            MinimaLogger::log("saveCascade: MiniData serialization returned null");
            return false;
        }

        checkOpen();
        openConnIfNeeded();

        const char* sql = "INSERT INTO cascadedata (cascadetip, fulldata) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("saveCascade prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return false;
        }

        long long tip = zCascade.getTip()->getTxPoW().getBlockNumber().getAsLong();
        rc = sqlite3_bind_int64(stmt, 1, tip);
        if (rc != SQLITE_OK) {
            MinimaLogger::log("saveCascade bind tip failed");
            sqlite3_finalize(stmt);
            return false;
        }

        const std::vector<std::uint8_t>& bytes = cascdata->getBytes();
        rc = sqlite3_bind_blob(stmt, 2, bytes.data(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            MinimaLogger::log("saveCascade bind blob failed");
            sqlite3_finalize(stmt);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("saveCascade step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return false;
    }
}

std::unique_ptr<Cascade> ArchiveManager::loadCascade() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT fulldata FROM cascadedata ORDER BY cascadetip ASC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadCascade prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        std::unique_ptr<Cascade> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData minisync(v);
                result = Cascade::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadCascade step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

bool ArchiveManager::saveBlock(TxBlock& zBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        if (_intSaveBlock(zBlock)) {
            return true;
        }
    } catch (...) {
        // Retry once
    }
    try {
        return _intSaveBlock(zBlock);
    } catch (...) {
        return false;
    }
}

bool ArchiveManager::_intSaveBlock(TxBlock& zBlock) {
    checkOpen();
    openConnIfNeeded();

    // Serialize block
    std::unique_ptr<MiniData> syncdata = MiniData::getMiniDataVersion(zBlock);
    if (!syncdata) {
        MinimaLogger::log("saveBlock: MiniData serialization returned null");
        return false;
    }

    const char* sql = "INSERT OR IGNORE INTO syncblock (txpowid, block, timemilli, syncdata) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        MinimaLogger::log(std::string("saveBlock prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        return false;
    }

    const std::string txpowid = zBlock.getTxPoW().getTxPoWID();
    const long long blocknum = zBlock.getTxPoW().getBlockNumber().getAsLong();
    const long long timemilli = nowMillis();

    rc = sqlite3_bind_text(stmt, 1, txpowid.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { MinimaLogger::log("saveBlock bind txpowid failed"); sqlite3_finalize(stmt); return false; }

    rc = sqlite3_bind_int64(stmt, 2, blocknum);
    if (rc != SQLITE_OK) { MinimaLogger::log("saveBlock bind block failed"); sqlite3_finalize(stmt); return false; }

    rc = sqlite3_bind_int64(stmt, 3, timemilli);
    if (rc != SQLITE_OK) { MinimaLogger::log("saveBlock bind timemilli failed"); sqlite3_finalize(stmt); return false; }

    const std::vector<std::uint8_t>& bytes = syncdata->getBytes();
    rc = sqlite3_bind_blob(stmt, 4, bytes.data(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { MinimaLogger::log("saveBlock bind blob failed"); sqlite3_finalize(stmt); return false; }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        MinimaLogger::log(std::string("saveBlock step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

std::unique_ptr<TxBlock> ArchiveManager::loadBlock(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT syncdata FROM syncblock WHERE txpowid=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadBlock prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_text(stmt, 1, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData minisync(v);
                result = TxBlock::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadBlock step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

std::unique_ptr<TxBlock> ArchiveManager::loadFirstBlock() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        // Get highest block number
        const char* sql1 = "SELECT block FROM syncblock ORDER BY block DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql1, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadFirstBlock prepare1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        long long block = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            block = sqlite3_column_int64(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadFirstBlock step1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_finalize(stmt);

        if (block == -1) return nullptr;

        const char* sql2 = "SELECT syncdata FROM syncblock WHERE block=?;";
        rc = sqlite3_prepare_v2(mConn, sql2, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadFirstBlock prepare2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_int64(stmt, 1, block);

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData minisync(v);
                result = TxBlock::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadFirstBlock step2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

std::unique_ptr<TxBlock> ArchiveManager::loadLastBlock() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        // Get lowest block number
        const char* sql1 = "SELECT block FROM syncblock ORDER BY block ASC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql1, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadLastBlock prepare1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        long long block = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            block = sqlite3_column_int64(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadLastBlock step1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return nullptr;
        }
        sqlite3_finalize(stmt);

        if (block == -1) return nullptr;

        const char* sql2 = "SELECT syncdata FROM syncblock WHERE block=?;";
        rc = sqlite3_prepare_v2(mConn, sql2, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadLastBlock prepare2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_int64(stmt, 1, block);

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData minisync(v);
                result = TxBlock::convertMiniDataVersion(minisync);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadLastBlock step2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

std::vector<std::unique_ptr<TxBlock>>
ArchiveManager::loadSyncBlockRange(const MiniNumber& zStartBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::unique_ptr<TxBlock>> blocks;

    try {
        checkOpen();
        openConnIfNeeded();

        // endblock = start - 256; if <= 1 -> 1
        MiniNumber endblock = zStartBlock.sub(MiniNumber::TWOFIVESIX());
        if (endblock.isLessEqual(MiniNumber::ONE())) {
            endblock = MiniNumber::ONE();
        }

        const char* sql = "SELECT syncdata FROM syncblock WHERE block<? AND block>=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadSyncBlockRange prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return blocks;
        }

        sqlite3_bind_int64(stmt, 1, zStartBlock.getAsLong());
        sqlite3_bind_int64(stmt, 2, endblock.getAsLong());

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            // Check shutdown
            auto mainptr = org::minima::system::Main::getInstance();
            if (mainptr && mainptr->isShuttingDown()) {
                break;
            }

            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData md(v);
                auto sb = TxBlock::convertMiniDataVersion(md);
                if (sb) {
                    blocks.push_back(std::move(sb));
                }
            }
        }

        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            MinimaLogger::log(std::string("loadSyncBlockRange step error: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);

        // Sort descending by block number
        std::sort(blocks.begin(), blocks.end(),
                  [](const std::unique_ptr<TxBlock>& a, const std::unique_ptr<TxBlock>& b) {
                      return b->getTxPoW().getBlockNumber().compareTo(a->getTxPoW().getBlockNumber()) < 0;
                  });

        return blocks;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return blocks;
    }
}

MiniNumber ArchiveManager::exists(const std::string& zTxPoWID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT block FROM syncblock WHERE txpowid=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("exists prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return MiniNumber::MINUSONE();
        }

        sqlite3_bind_text(stmt, 1, zTxPoWID.c_str(), -1, SQLITE_TRANSIENT);

        MiniNumber result = MiniNumber::MINUSONE();
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            long long block = sqlite3_column_int64(stmt, 0);
            result = MiniNumber(block);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("exists step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return MiniNumber::MINUSONE();
    }
}

std::vector<std::unique_ptr<TxBlock>>
ArchiveManager::loadBlockRange(const MiniNumber& zStartBlock, const MiniNumber& zEndBlock) {
    return loadBlockRange(zStartBlock, zEndBlock, true);
}

std::vector<std::unique_ptr<TxBlock>>
ArchiveManager::loadBlockRange(const MiniNumber& zStartBlock, const MiniNumber& zEndBlock, bool zDescending) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::unique_ptr<TxBlock>> blocks;

    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT syncdata FROM syncblock WHERE block>? AND block<?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadBlockRange prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return blocks;
        }

        sqlite3_bind_int64(stmt, 1, zStartBlock.getAsLong());
        sqlite3_bind_int64(stmt, 2, zEndBlock.getAsLong());

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData md(v);
                auto sb = TxBlock::convertMiniDataVersion(md);
                if (sb) {
                    blocks.push_back(std::move(sb));
                }
            }
        }

        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            MinimaLogger::log(std::string("loadBlockRange step error: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);

        if (zDescending) {
            std::sort(blocks.begin(), blocks.end(),
                      [](const std::unique_ptr<TxBlock>& a, const std::unique_ptr<TxBlock>& b) {
                          return b->getTxPoW().getBlockNumber().compareTo(a->getTxPoW().getBlockNumber()) < 0;
                      });
        } else {
            std::sort(blocks.begin(), blocks.end(),
                      [](const std::unique_ptr<TxBlock>& a, const std::unique_ptr<TxBlock>& b) {
                          return a->getTxPoW().getBlockNumber().compareTo(b->getTxPoW().getBlockNumber()) < 0;
                      });
        }

        return blocks;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return blocks;
    }
}

std::unique_ptr<TxBlock>
ArchiveManager::loadBlockFromNumber(const MiniNumber& zBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        const char* sql = "SELECT syncdata FROM syncblock WHERE block=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("loadBlockFromNumber prepare failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return nullptr;
        }

        sqlite3_bind_int64(stmt, 1, zBlock.getAsLong());

        std::unique_ptr<TxBlock> result;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blen = sqlite3_column_bytes(stmt, 0);
            if (blob && blen > 0) {
                std::vector<std::uint8_t> v(static_cast<const std::uint8_t*>(blob),
                                            static_cast<const std::uint8_t*>(blob) + blen);
                MiniData md(v);
                result = TxBlock::convertMiniDataVersion(md);
            }
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("loadBlockFromNumber step failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
        }

        sqlite3_finalize(stmt);
        return result;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return nullptr;
    }
}

int ArchiveManager::checkForCleanDB() {
    if (!GeneralParams::ARCHIVE) {
        return cleanDB();
    }
    return 0;
}

int ArchiveManager::cleanDB() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        checkOpen();
        openConnIfNeeded();

        // Get highest block number
        const char* sql1 = "SELECT block FROM syncblock ORDER BY block DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(mConn, sql1, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("cleanDB prepare1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return 0;
        }

        long long block = -1;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            block = sqlite3_column_int64(stmt, 0);
        } else if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("cleanDB step1 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return 0;
        }
        sqlite3_finalize(stmt);

        if (block == -1) {
            return 0;
        }

        MiniNumber cutoff = MiniNumber(block).sub(MiniNumber(MAX_KEEP_BLOCKS));

        const char* sql2 = "DELETE FROM syncblock WHERE block < ?;";
        rc = sqlite3_prepare_v2(mConn, sql2, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MinimaLogger::log(std::string("cleanDB prepare2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            return 0;
        }

        sqlite3_bind_int64(stmt, 1, cutoff.getAsLong());

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            MinimaLogger::log(std::string("cleanDB step2 failed: ") + (mConn ? sqlite3_errmsg(mConn) : "no db"));
            sqlite3_finalize(stmt);
            return 0;
        }

        int changes = sqlite3_changes(mConn);
        sqlite3_finalize(stmt);
        return changes;
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
        return 0;
    }
}

void ArchiveManager::hackShut() {
    // Close our SQLite handle and base DB
    if (mConn) {
        sqlite3_close(mConn);
        mConn = nullptr;
    }
    hardCloseDB();
}

void ArchiveManager::closeAndReopen() {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        // First close the connection and save DB.. no compact
        saveDB(false);

        // Close our own connection if open
        if (mConn) {
            sqlite3_close(mConn);
            mConn = nullptr;
        }

        // Re-open the base DB (no logs)
        checkOpen(false);

        // Our own handle will be reopened lazily on next operation
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
}

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org