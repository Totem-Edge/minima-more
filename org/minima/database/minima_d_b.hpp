#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>
#include <condition_variable>
#include <chrono>

#include "org/minima/utils/minima_logger.hpp"

/**
 * ReentrantReadWriteLock equivalent for C++.
 *
 * Matches Java's ReentrantReadWriteLock default (non-fair) semantics:
 *  - Multiple reader threads can hold the lock concurrently.
 *  - Read locks are reentrant for the same thread.
 *  - The write lock is exclusive and NOT reentrant (no nested write locks).
 *  - While a writer is waiting, new readers block to avoid writer starvation.
 *
 * Diagnostics: if acquiring a lock takes longer than 30 seconds a warning is
 * logged. This helps detect lock contention / deadlock in production runs.
 */
class ReentrantRWLock {
private:
    mutable std::mutex m_mutex;
    std::condition_variable m_readersOk;
    std::condition_variable m_writersOk;

    bool m_writeLocked = false;
    std::thread::id m_writerThread;
    int m_writeHoldCount = 0;
    std::unordered_map<std::thread::id, int> m_readHolds;
    int m_waitingWriters = 0;
    int m_waitingReaders = 0;

    bool canReadLock(std::thread::id tid) const {
        // Writers always have implicit read access while holding the write lock.
        if (m_writeLocked && m_writerThread == tid) {
            return true;
        }
        // If no writer holds or waits, readers can proceed.
        if (!m_writeLocked && m_waitingWriters == 0) {
            return true;
        }
        // If a writer is waiting but this thread already has a read lock,
        // allow reentrancy so readers don't self-deadlock.
        if (!m_writeLocked && m_readHolds.find(tid) != m_readHolds.end()) {
            return true;
        }
        return false;
    }

    bool canWriteLock(std::thread::id tid) const {
        // Reentrant write lock for the same thread.
        if (m_writeLocked && m_writerThread == tid) {
            return true;
        }
        // Otherwise exclusive: nobody else may hold write or read locks.
        if (m_writeLocked) {
            return false;
        }
        int totalReads = 0;
        for (const auto& kv : m_readHolds) {
            totalReads += kv.second;
        }
        return totalReads == 0;
    }

    void logSlowLock(const std::string& zType, long long zMillis) const {
        org::minima::utils::MinimaLogger::log(
            "[ReentrantRWLock] SLOW " + zType + " LOCK took " + std::to_string(zMillis) + "ms"
            + " writerLocked=" + std::to_string(m_writeLocked)
            + " waitingWriters=" + std::to_string(m_waitingWriters)
            + " waitingReaders=" + std::to_string(m_waitingReaders)
            + " readerThreads=" + std::to_string(m_readHolds.size()));
    }

public:
    void lock() {
        auto tid = std::this_thread::get_id();
        std::unique_lock<std::mutex> lk(m_mutex);
        bool alreadyHeld = (m_writeLocked && m_writerThread == tid);

        m_waitingWriters++;
        auto start = std::chrono::steady_clock::now();
        m_writersOk.wait(lk, [this, tid]() { return canWriteLock(tid); });
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        m_waitingWriters--;
        if (elapsed > 30000) {
            logSlowLock("WRITE", elapsed);
        }
        m_writeLocked = true;
        m_writerThread = tid;
        m_writeHoldCount++;

        if (alreadyHeld) {
            static thread_local bool warned = false;
            if (!warned) {
                org::minima::utils::MinimaLogger::log(
                    "[ReentrantRWLock] Reentrant write lock detected on same thread");
                warned = true;
            }
        }
    }

    void unlock() {
        auto tid = std::this_thread::get_id();
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_writeLocked || m_writerThread != tid) {
            // Match Java's forgiving unlock behavior (IllegalMonitorStateException
            // is caught in safeReleaseReadWriteLock). Silently ignore.
            return;
        }
        m_writeHoldCount--;
        if (m_writeHoldCount > 0) {
            return;
        }
        m_writeLocked = false;
        m_writerThread = {};
        if (m_waitingWriters > 0) {
            m_writersOk.notify_one();
        } else if (m_waitingReaders > 0) {
            m_readersOk.notify_all();
        }
    }

    void lock_shared() {
        auto tid = std::this_thread::get_id();
        std::unique_lock<std::mutex> lk(m_mutex);
        m_waitingReaders++;
        auto start = std::chrono::steady_clock::now();
        m_readersOk.wait(lk, [this, tid]() { return canReadLock(tid); });
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        m_waitingReaders--;
        if (elapsed > 30000) {
            logSlowLock("READ", elapsed);
        }
        m_readHolds[tid]++;
    }

    void unlock_shared() {
        auto tid = std::this_thread::get_id();
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_readHolds.find(tid);
        if (it == m_readHolds.end()) {
            // Not holding read lock; ignore (Java forgiving behavior).
            return;
        }
        if (--(it->second) <= 0) {
            m_readHolds.erase(it);
        }
        if (m_waitingWriters > 0 && m_readHolds.empty()) {
            m_writersOk.notify_one();
        } else if (m_waitingReaders > 0 && m_waitingWriters == 0) {
            m_readersOk.notify_all();
        }
    }

    int getWriteHoldCount() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return (m_writeLocked && m_writerThread == std::this_thread::get_id()) ? m_writeHoldCount : 0;
    }

    int getReadHoldCount() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_readHolds.find(std::this_thread::get_id());
        return (it != m_readHolds.end()) ? it->second : 0;
    }

    bool isWriteLockedByCurrentThread() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_writeLocked && m_writerThread == std::this_thread::get_id();
    }

    bool isWriteLocked() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_writeLocked;
    }

    int waitingWriters() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_waitingWriters;
    }

    int activeReaders() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        int total = 0;
        for (const auto& kv : m_readHolds) {
            total += kv.second;
        }
        return total;
    }
};

// Forward declarations (namespaced per Pitfall 10)
namespace org { namespace minima { namespace database { namespace archive { class ArchiveManager; class TxBlockDB; } } } }
namespace org { namespace minima { namespace database { namespace txpowdb { class TxPoWDB; } } } }
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPowTree; } } } }
namespace org { namespace minima { namespace database { namespace cascade { class Cascade; } } } }
namespace org { namespace minima { namespace database { namespace userprefs { class UserDB; } } } }
namespace org { namespace minima { namespace database { namespace userprefs { namespace txndb { class TxnDB; } } } } }
namespace org { namespace minima { namespace database { namespace wallet { class Wallet; } } } }
namespace org { namespace minima { namespace system { namespace network { namespace p2p { class P2PDB; } } } } }
namespace org { namespace minima { namespace system { namespace network { namespace p2p2 { class P2P2DB; } } } } }
namespace org { namespace minima { namespace objects { namespace mmr { class MegaMMR; } } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace database {

class MinimaDB {
public:
    // Singleton access (Java: static mMinimaDB with create/get)
    static void createDB();
    static MinimaDB* getDB();

    // PIMPL-fix special members due to unique_ptr to incomplete types
    virtual ~MinimaDB();
    MinimaDB(MinimaDB&&) noexcept;
    MinimaDB& operator=(MinimaDB&&) noexcept;

    // Delete copy
    MinimaDB(const MinimaDB&) = delete;
    MinimaDB& operator=(const MinimaDB&) = delete;

    // Locking API
    void readLock(bool zLock);
    void writeLock(bool zLock);
    void safeReleaseReadWriteLock();
    std::string getRWLockInfo() const;

    // Database getters (references guaranteed valid after construction)
    org::minima::database::txpowdb::TxPoWDB& getTxPoWDB();
    org::minima::database::archive::TxBlockDB& getTxBlockDB();
    org::minima::database::txpowtree::TxPowTree& getTxPoWTree();
    org::minima::objects::mmr::MegaMMR& getMegaMMR();
    void hardSetMegaMMR(org::minima::objects::mmr::MegaMMR& zMEGA);
    org::minima::database::cascade::Cascade& getCascade();
    void setIBDCascade(const org::minima::database::cascade::Cascade& zCascade);
    void resetCascadeAndTxPoWTree();

    long long getCascadeFileSize();
    long long getUserDBFileSize();
    long long getTxPowTreeFileSize();
    long long getP2PFileSize();

    std::filesystem::path getCascadeFile();
    std::filesystem::path getDBFile(const std::string& zFilename);

    org::minima::database::userprefs::UserDB& getUserDB();
    org::minima::database::userprefs::txndb::TxnDB& getCustomTxnDB();
    org::minima::database::wallet::Wallet& getWallet();
    org::minima::database::archive::ArchiveManager& getArchive();
    org::minima::system::network::p2p::P2PDB& getP2PDB();
    org::minima::system::network::p2p2::P2P2DB& getP2P2DB();

    std::filesystem::path getBaseDBFolder();

    // Loaders
    void loadAllDB();
    void loadArchiveAndTxPoWDB(bool zResetWallet);
    void loadDBsForRestoreSync();

    // SQL refresh/save
    void refreshSQLDB();
    void saveAllDB();
    void saveAllDB(bool zCompact);
    void saveSQL(bool zCompact);

    // Restart / Memory cleanup
    void fullDBRestartMemFree();

    // Save state toggles and saves
    void setAllowSaveState(bool zAllow);
    void saveState();
    void saveUserDB();
    void saveP2PDB();

    // Coin notify management
    void addCoinNotify(const std::string& zAddress);
    bool removeCoinNotify(const std::string& zAddress);
    bool checkCoinNotify(const std::string& zAddress) const;

    // Static info copied from Java
    static std::string mCurrentWriteLockThread;
    static bool mCurrentWriteLockState;
    static thread_local std::string sThreadName;
    static std::string getCurrentThreadName();
    static void setCurrentThreadName(const std::string& name);

private:
    MinimaDB(); // constructor is private; use createDB()

    long long getDBFileSizeInternal(const std::string& zFilename) const;

private:
    // Singleton instance
    static std::unique_ptr<MinimaDB> sMinimaDB;

    // The individual DBs
    std::unique_ptr<org::minima::database::archive::ArchiveManager>    mArchive;
    std::unique_ptr<org::minima::database::txpowdb::TxPoWDB>           mTxPoWDB;
    std::unique_ptr<org::minima::database::txpowtree::TxPowTree>       mTxPoWTree;
    std::unique_ptr<org::minima::database::cascade::Cascade>           mCascade;
    std::unique_ptr<org::minima::database::userprefs::UserDB>          mUserDB;
    std::unique_ptr<org::minima::database::userprefs::txndb::TxnDB>    mTxnDB;
    std::unique_ptr<org::minima::database::wallet::Wallet>             mWallet;
    std::unique_ptr<org::minima::database::archive::TxBlockDB>         mTxBlockDB;

    // MEGA MMR shared like Java references
    std::shared_ptr<org::minima::objects::mmr::MegaMMR>                mMegaMMR;

    // P2P
    std::unique_ptr<org::minima::system::network::p2p::P2PDB>          mP2PDB;
    std::unique_ptr<org::minima::system::network::p2p2::P2P2DB>        mP2P2DB;

    // Locking for read/write ops
    mutable ReentrantRWLock mRWLock;

    // Coin notification set (thread-safe with mutex)
    mutable std::mutex mCoinNotifyMutex;
    std::unordered_set<std::string> mCoinNotify;

    // Save state allowed
    bool mAllowSaveState {true};

    //cascade tree desync
    void rebuildTreeFromCascade();
    bool validateCascadeTreeAlignment() const;
};

} // namespace database
} // namespace minima
} // namespace org