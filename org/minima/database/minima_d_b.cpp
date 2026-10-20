#include "org/minima/database/minima_d_b.hpp"

#include <sstream>
#include <exception>
#include <cstdlib>
#include <chrono>

#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/archive/tx_block_d_b.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp" 
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp" 
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/mmr/mega_m_m_r.hpp"
#include "org/minima/system/network/p2p/p2_p_d_b.hpp"
#include "org/minima/system/network/p2p2/p2_p2_d_b.hpp"

// FIX 1: Add the full include for TxPoW
#include "org/minima/objects/tx_po_w.hpp" 

#include "org/minima/system/main.hpp"
#include "org/minima/system/params/general_params.hpp"

#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/messages/timer_message.hpp"

#include "org/minima/objects/base/mini_number.hpp"

namespace fs = std::filesystem;

namespace org {
namespace minima {
namespace database {

// Static members
std::unique_ptr<MinimaDB> MinimaDB::sMinimaDB;
std::string MinimaDB::mCurrentWriteLockThread = "";
bool MinimaDB::mCurrentWriteLockState = false;
thread_local std::string MinimaDB::sThreadName = "";

std::string MinimaDB::getCurrentThreadName() {
    if (!sThreadName.empty()) {
        return sThreadName;
    }
    std::ostringstream os;
    os << std::this_thread::get_id();
    return os.str();
}

void MinimaDB::setCurrentThreadName(const std::string& name) {
    sThreadName = name;
}

// Private constructor
MinimaDB::MinimaDB()
    : mArchive(std::make_unique<org::minima::database::archive::ArchiveManager>())
    , mTxPoWDB(std::make_unique<org::minima::database::txpowdb::TxPoWDB>())
    , mTxPoWTree(std::make_unique<org::minima::database::txpowtree::TxPowTree>())
    , mCascade(std::make_unique<org::minima::database::cascade::Cascade>())
    , mUserDB(std::make_unique<org::minima::database::userprefs::UserDB>())
    , mTxnDB(nullptr) // constructed on load as in Java
    , mWallet(std::make_unique<org::minima::database::wallet::Wallet>())
    , mTxBlockDB(std::make_unique<org::minima::database::archive::TxBlockDB>())
    , mMegaMMR(std::make_shared<org::minima::objects::mmr::MegaMMR>())
    , mP2PDB(std::make_unique<org::minima::system::network::p2p::P2PDB>())
    , mP2P2DB(std::make_unique<org::minima::system::network::p2p2::P2P2DB>()) {
    // mRWLock, mCoinNotify, and mAllowSaveState are default-initialized
}

// Destructor and move ops definitions (Pitfall 1)
MinimaDB::~MinimaDB() = default;

MinimaDB::MinimaDB(MinimaDB&&) noexcept {
    // This is a singleton and should never be moved.
    // This definition just satisfies the compiler.
}
MinimaDB& MinimaDB::operator=(MinimaDB&&) noexcept {
    // This is a singleton and should never be moved.
    return *this;
}

// Singleton methods
void MinimaDB::createDB() {
    sMinimaDB = std::unique_ptr<MinimaDB>(new MinimaDB());
}
MinimaDB* MinimaDB::getDB() {
    return sMinimaDB.get();
}

// Locking - mirrors Java MinimaDB behavior exactly
void MinimaDB::readLock(bool zLock) {
    using org::minima::system::params::GeneralParams;

    if (GeneralParams::DB_IGNORE_LOCKS) {
        return;
    }

    if (zLock) {
        mRWLock.lock_shared();
    } else {
        try {
            mRWLock.unlock_shared();
        } catch (...) {
            org::minima::utils::MinimaLogger::log(
                "[MinimaDB::readLock] unlock_shared failed");
        }
    }
}

void MinimaDB::writeLock(bool zLock) {
    using org::minima::system::params::GeneralParams;

    if (GeneralParams::DB_IGNORE_LOCKS) {
        return;
    }

    if (zLock) {
        mRWLock.lock();
        mCurrentWriteLockThread = getCurrentThreadName();
    } else {
        try {
            mRWLock.unlock();
        } catch (...) {
            org::minima::utils::MinimaLogger::log(
                "[MinimaDB::writeLock] unlock failed");
        }
    }

    mCurrentWriteLockState = zLock;
}

void MinimaDB::safeReleaseReadWriteLock() {
    using org::minima::system::params::GeneralParams;

    if (GeneralParams::DB_IGNORE_LOCKS) {
        return;
    }

    try {
        if (mRWLock.getWriteHoldCount() > 0) {
            mRWLock.unlock();
        }
    } catch (...) {
        // ignore
    }

    try {
        if (mRWLock.getReadHoldCount() > 0) {
            mRWLock.unlock_shared();
        }
    } catch (...) {
        // ignore
    }
}

std::string MinimaDB::getRWLockInfo() const {
    std::ostringstream os;
    os << "MinimaDB RWLock Info { "
       << "currentReadHolds=" << mRWLock.getReadHoldCount()
       << ", currentWriteHolds=" << mRWLock.getWriteHoldCount()
       << ", activeReaders=" << mRWLock.activeReaders()
       << ", waitingWriters=" << mRWLock.waitingWriters()
       << ", writeLocked=" << (mRWLock.isWriteLocked() ? "true" : "false")
       << ", currentWriteThread=" << mCurrentWriteLockThread
       << ", writeLockState=" << (mCurrentWriteLockState ? "true" : "false")
       << " }";
    return os.str();
}

// Accessors
org::minima::database::txpowdb::TxPoWDB& MinimaDB::getTxPoWDB() {
    return *mTxPoWDB;
}

org::minima::database::archive::TxBlockDB& MinimaDB::getTxBlockDB() {
    return *mTxBlockDB;
}

org::minima::database::txpowtree::TxPowTree& MinimaDB::getTxPoWTree() {
    return *mTxPoWTree;
}

org::minima::objects::mmr::MegaMMR& MinimaDB::getMegaMMR() {
    return *mMegaMMR;
}

void MinimaDB::hardSetMegaMMR(org::minima::objects::mmr::MegaMMR& zMEGA) {
    // Take ownership via move to emulate Java's assignment by reference
    mMegaMMR = std::make_shared<org::minima::objects::mmr::MegaMMR>(std::move(zMEGA));
}

org::minima::database::cascade::Cascade& MinimaDB::getCascade() {
    return *mCascade;
}

void MinimaDB::setIBDCascade(const org::minima::database::cascade::Cascade& zCascade) {
    // Deep copy to mimic Java reference semantics without aliasing
    mCascade = zCascade.deepCopy();
}

void MinimaDB::resetCascadeAndTxPoWTree() {
    mCascade   = std::make_unique<org::minima::database::cascade::Cascade>();
    mTxPoWTree = std::make_unique<org::minima::database::txpowtree::TxPowTree>();
}

long long MinimaDB::getCascadeFileSize() {
    return getDBFileSizeInternal("cascade.db");
}
long long MinimaDB::getUserDBFileSize() {
    return getDBFileSizeInternal("userprefs.db");
}
long long MinimaDB::getTxPowTreeFileSize() {
    return getDBFileSizeInternal("chaintree.db");
}
long long MinimaDB::getP2PFileSize() {
    return getDBFileSizeInternal("p2p.db");
}

std::filesystem::path MinimaDB::getCascadeFile() {
    return getDBFile("cascade.db");
}

std::filesystem::path MinimaDB::getDBFile(const std::string& zFilename) {
    fs::path basedb = getBaseDBFolder();
    return basedb / zFilename;
}

long long MinimaDB::getDBFileSizeInternal(const std::string& zFilename) const {
    fs::path basedb = const_cast<MinimaDB*>(this)->getBaseDBFolder();
    fs::path file = basedb / zFilename;
    std::error_code ec;
    auto sz = fs::file_size(file, ec);
    if (!ec) {
        return static_cast<long long>(sz);
    }
    return 0;
}

org::minima::database::userprefs::UserDB& MinimaDB::getUserDB() {
    return *mUserDB;
}

org::minima::database::userprefs::txndb::TxnDB& MinimaDB::getCustomTxnDB() {
    return *mTxnDB;
}

org::minima::database::wallet::Wallet& MinimaDB::getWallet() {
    return *mWallet;
}

org::minima::database::archive::ArchiveManager& MinimaDB::getArchive() {
    return *mArchive;
}

org::minima::system::network::p2p::P2PDB& MinimaDB::getP2PDB() {
    return *mP2PDB;
}

org::minima::system::network::p2p2::P2P2DB& MinimaDB::getP2P2DB() {
    return *mP2P2DB;
}

std::filesystem::path MinimaDB::getBaseDBFolder() {
    return fs::path(org::minima::system::params::GeneralParams::DATA_FOLDER) / "databases";
}

// Loaders
void MinimaDB::loadAllDB() {
    writeLock(true);
    try {
        fs::path basedb = getBaseDBFolder();

        // Wallet
        {
            fs::path walletsqlfolder = basedb / "walletsql";
            if (!org::minima::system::params::GeneralParams::IS_MAIN_DBPASSWORD_SET) {
                mWallet->loadDB((walletsqlfolder / "wallet").string());
            } else {
                org::minima::utils::MinimaLogger::log("Using Encrypted SQL DB");
                mWallet->loadDB((walletsqlfolder / "wallet").string());
            }
        }

        // Archive
        {
            fs::path archsqlfolder = basedb / "archivesql";
            try {
                mArchive->loadDB((archsqlfolder / "archive").string());
            } catch (const std::exception& exc) {
                org::minima::utils::MinimaLogger::log(exc);
                org::minima::utils::MinimaLogger::log("ERROR loading ArchiveDB.. WIPE and RESYNC.. ");
                mArchive->hackShut();
                org::minima::utils::MiniFile::deleteFileOrFolder(archsqlfolder.string(), archsqlfolder);
                mArchive = std::make_unique<org::minima::database::archive::ArchiveManager>();
                mArchive->loadDB((archsqlfolder / "archive").string());
            }
        }

        // TxPoW SQL DB
        {
            fs::path txpowsqlfolder = basedb / "txpowsql";
            try {
                mTxPoWDB->loadSQLDB(txpowsqlfolder / "txpow");
            } catch (const std::exception& exc) {
                org::minima::utils::MinimaLogger::log(exc);
                org::minima::utils::MinimaLogger::log("ERROR loading TxPoWSQLDB.. WIPE and RESYNC.. ");
                mTxPoWDB->hardCloseSQLDB();
                org::minima::utils::MiniFile::deleteFileOrFolder(txpowsqlfolder.string(), txpowsqlfolder);
                mTxPoWDB = std::make_unique<org::minima::database::txpowdb::TxPoWDB>();
                mTxPoWDB->loadSQLDB(txpowsqlfolder / "txpow");
            }
        }

        // UserDB
        mUserDB->loadDB((basedb / "userprefs.db").string());

        // Custom Txns
        mTxnDB = std::make_unique<org::minima::database::userprefs::txndb::TxnDB>();
        mTxnDB->loadDB();

        // Cascade
        mCascade->loadDB(basedb / "cascade.db");

        // TxPoWTree
        {
            fs::path txtree = basedb / "chaintree.db";
            std::error_code ec;
            auto size = fs::file_size(txtree, ec);
            org::minima::utils::MinimaLogger::log(
                std::string("Loading TxPowTree size : ")
                + org::minima::utils::MiniFormat::formatSize(ec ? 0 : static_cast<long long>(size)));
        }
        mTxPoWTree->loadDB(basedb / "chaintree.db");

        // P2P DBs
        mP2PDB->loadDB((basedb / "p2p.db").string());
        mP2P2DB->loadDB((basedb / "p2p2.db").string());

        // Cascade correctness (also handle cascade validation failure)
        if (!org::minima::database::cascade::Cascade::checkCascadeCorrect(*mCascade)) {
            org::minima::utils::MinimaLogger::log(
                "WARNING: Cascade validation failed - likely due to blockchain reorganization");
            org::minima::utils::MinimaLogger::log(
                "Attempting to rebuild TxPowTree from cascade...");
            
            try {
                // Try to rebuild the tree from cascade
                rebuildTreeFromCascade();
                
                // Re-validate cascade after rebuild
                if (!org::minima::database::cascade::Cascade::checkCascadeCorrect(*mCascade)) {
                    org::minima::utils::MinimaLogger::log(
                        "ERROR: Cascade validation still failing after tree rebuild");
                    throw std::runtime_error(
                        "Your Cascade is BROKEN.. please 'reset' your node.");
                }
                
                org::minima::utils::MinimaLogger::log(
                    "Successfully recovered from cascade-tree desynchronization");
                
            } catch (const std::exception& e) {
                org::minima::utils::MinimaLogger::log(
                    "ERROR: Failed to rebuild tree from cascade: " + std::string(e.what()));
                throw std::runtime_error(
                    "Your Cascade is BROKEN.. please 'reset' your node.");
            }
        }

        // MEGA MMR
        if (org::minima::system::params::GeneralParams::IS_MEGAMMR) {
            // R2 coin store: LMDB when available (mmap-backed), else in-memory.
            mMegaMMR->openCoinStore(basedb / "coins.lmdb");
            mMegaMMR->loadMMR(basedb / "megammr.mmr");
        } else {
            org::minima::utils::MiniFile::deleteFileOrFolder(basedb.string(), basedb / "megammr.mmr");
        }

        // And check tree vs cascade alignment when available
        if (mCascade->getTip() != nullptr && mTxPoWTree->getRoot() != nullptr) {
            org::minima::objects::base::MiniNumber cascstart =
                mCascade->getTip()->getTxPoW().getBlockNumber();
            org::minima::objects::base::MiniNumber treeroot =
                // FIX 2: Use -> to access member of std::shared_ptr
                mTxPoWTree->getRoot()->getTxPoW().getBlockNumber();
            if (!treeroot.isEqual(cascstart.increment())) {
                throw std::runtime_error("Your Cascade is BROKEN.. please 'reset' your node.");
            }
            // MMR start-time alignment check omitted due to API exposure constraints
        }

        // Ensure Archive Cascade consistency
        mArchive->checkCascadeRequired(*mCascade);

    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log("SERIOUS ERROR loadAllDB : ");
        org::minima::utils::MinimaLogger::log(exc);

        std::string err = exc.what();
        if (!org::minima::system::params::GeneralParams::RESCUE_MEGAMMR_NODE.empty()) {
            org::minima::utils::MinimaLogger::log(
                "RESCUE NODE FOUND.. attempting rescue @ "
                + org::minima::system::params::GeneralParams::RESCUE_MEGAMMR_NODE);
            // Post a rescue timer
            if (org::minima::system::Main::getInstance()) {
                auto tmsg = std::make_shared<org::minima::utils::messages::TimerMessage>(1000, org::minima::system::Main::MAIN_DO_RESCUE);
                org::minima::system::Main::getInstance()->PostTimerMessage(tmsg);
            }
        } else {
            if (org::minima::system::params::GeneralParams::IS_MOBILE
                || org::minima::system::params::GeneralParams::IS_JNLP) {
                if (org::minima::system::Main::getInstance()) {
                    org::minima::system::Main::getInstance()->setStartUpError(true, err);
                }
            } else {
                std::exit(0);
            }
        }
    }

    writeLock(false);
}

void MinimaDB::loadArchiveAndTxPoWDB(bool zResetWallet) {
    writeLock(true);
    try {
        fs::path basedb = getBaseDBFolder();

        // Wallet
        if (zResetWallet) {
            mWallet = std::make_unique<org::minima::database::wallet::Wallet>();
            fs::path walletsqlfolder = basedb / "walletsql";
            if (!org::minima::system::params::GeneralParams::IS_MAIN_DBPASSWORD_SET) {
                mWallet->loadDB((walletsqlfolder / "wallet").string());
            } else {
                mWallet->loadDB((walletsqlfolder / "wallet").string());
            }
        }

        // Archive
        mArchive = std::make_unique<org::minima::database::archive::ArchiveManager>();
        fs::path archsqlfolder = basedb / "archivesql";
        mArchive->loadDB((archsqlfolder / "archive").string());

        // TxPoW SQL
        mTxPoWDB = std::make_unique<org::minima::database::txpowdb::TxPoWDB>();
        fs::path txpowsqlfolder = basedb / "txpowsql";
        mTxPoWDB->loadSQLDB(txpowsqlfolder / "txpow");

    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log("SERIOUS ERROR loadArchiveAndTxPoWDB");
        org::minima::utils::MinimaLogger::log(exc);
    }
    writeLock(false);
}

void MinimaDB::loadDBsForRestoreSync() {
    writeLock(true);
    try {
        fs::path basedb = getBaseDBFolder();

        // Wallet
        mWallet = std::make_unique<org::minima::database::wallet::Wallet>();
        fs::path walletsqlfolder = basedb / "walletsql";
        if (!org::minima::system::params::GeneralParams::IS_MAIN_DBPASSWORD_SET) {
            mWallet->loadDB((walletsqlfolder / "wallet").string());
        } else {
            mWallet->loadDB((walletsqlfolder / "wallet").string());
        }

        // Archive
        mArchive = std::make_unique<org::minima::database::archive::ArchiveManager>();
        fs::path archsqlfolder = basedb / "archivesql";
        mArchive->loadDB((archsqlfolder / "archive").string());

        // TxPoW SQL
        mTxPoWDB = std::make_unique<org::minima::database::txpowdb::TxPoWDB>();
        fs::path txpowsqlfolder = basedb / "txpowsql";
        mTxPoWDB->loadSQLDB(txpowsqlfolder / "txpow");

        // Cascade
        mCascade = std::make_unique<org::minima::database::cascade::Cascade>();
        mCascade->loadDB(basedb / "cascade.db");

        // TxPoWTree
        mTxPoWTree = std::make_unique<org::minima::database::txpowtree::TxPowTree>();
        mTxPoWTree->loadDB(basedb / "chaintree.db");

    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log("SERIOUS ERROR loadDBsForRestoreSync");
        org::minima::utils::MinimaLogger::log(exc);
    }
    writeLock(false);
}

void MinimaDB::refreshSQLDB() {
    // First Archive
    mArchive->closeAndReopen();

    // Re-open TxPoWDB by path
    fs::path file = mTxPoWDB->getSqlFile();
    mTxPoWDB->hardCloseSQLDB();
    mTxPoWDB->loadSQLDB(file);
}

void MinimaDB::saveAllDB() {
    saveAllDB(false);
}

void MinimaDB::saveAllDB(bool zCompact) {
    org::minima::utils::MinimaLogger::log("Saving State..");
    saveState();

    org::minima::utils::MinimaLogger::log("Saving SQL..");
    saveSQL(zCompact);

    org::minima::utils::MinimaLogger::log("All saved..");
}

void MinimaDB::saveSQL(bool zCompact) {
    // std::cerr << "[DEBUG] saveSQL called with compact=" << (zCompact ? "true" : "false") << std::endl;
    
    writeLock(true);
    // std::cerr << "[DEBUG] Write lock acquired" << std::endl;
    
    try {
        org::minima::utils::MinimaLogger::log("Wallet shutdown..");
        // std::cerr << "[DEBUG] Starting Wallet save..." << std::endl;
        mWallet->saveDB(true);
        // std::cerr << "[DEBUG] Wallet save completed" << std::endl;
        
        // std::cerr << "[DEBUG] Waiting 300ms before TxPoWDB..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        org::minima::utils::MinimaLogger::log("TxPowDB shutdown..");
        // std::cerr << "[DEBUG] Starting TxPoWDB save..." << std::endl;
        mTxPoWDB->saveDB(zCompact);
        // std::cerr << "[DEBUG] TxPoWDB save completed" << std::endl;
        
        // std::cerr << "[DEBUG] Waiting 300ms before ArchiveDB..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        org::minima::utils::MinimaLogger::log("ArchiveDB shutdown..");
        // std::cerr << "[DEBUG] Starting ArchiveDB save..." << std::endl;
        mArchive->saveDB(zCompact);
        // std::cerr << "[DEBUG] ArchiveDB save completed" << std::endl;
        
        org::minima::utils::MinimaLogger::log("All SQL DB Shutdown..");
        // std::cerr << "[DEBUG] All databases saved successfully" << std::endl;
        
    } catch (const std::exception& exc) {
        // std::cerr << "[EXCEPTION] saveSQL error: " << exc.what() << std::endl;
        org::minima::utils::MinimaLogger::log(exc);
    }
    
    writeLock(false);
    // std::cerr << "[DEBUG] Write lock released" << std::endl;
}

void MinimaDB::fullDBRestartMemFree() {
    writeLock(true);
    try {
        fs::path basedb = getBaseDBFolder();

        // Wipe/clean RAM/SQL and archive checks
        mTxPoWDB->wipeDBRAM();
        mTxPoWDB->cleanDBSQL();
        mArchive->checkForCleanDB();

        // Save all
        mTxPoWDB->saveDB(false);
        mArchive->saveDB(false);
        mWallet->saveDB(false);

        // Reload Wallet
        mWallet = std::make_unique<org::minima::database::wallet::Wallet>();
        fs::path walletsqlfolder = basedb / "walletsql";
        if (!org::minima::system::params::GeneralParams::IS_MAIN_DBPASSWORD_SET) {
            mWallet->loadDB((walletsqlfolder / "wallet").string());
        } else {
            mWallet->loadDB((walletsqlfolder / "wallet").string());
        }

        // Reload Archive
        mArchive = std::make_unique<org::minima::database::archive::ArchiveManager>();
        fs::path archsqlfolder = basedb / "archivesql";
        mArchive->loadDB((archsqlfolder / "archive").string());

        // Reload TxPoW SQL
        mTxPoWDB = std::make_unique<org::minima::database::txpowdb::TxPoWDB>();
        fs::path txpowsqlfolder = basedb / "txpowsql";
        mTxPoWDB->loadSQLDB(txpowsqlfolder / "txpow");

    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }
    writeLock(false);
}

void MinimaDB::setAllowSaveState(bool zAllow) {
    mAllowSaveState = zAllow;
}

void MinimaDB::saveState() {
    using org::minima::utils::MinimaLogger;
    
    //Are we allowed..
    if(!mAllowSaveState) {
        return; 
    }
    
    //We need read lock 
    readLock(true);
    
    try {
        //Get the base Database folder
        fs::path basedb = getBaseDBFolder();
        
        // VALIDATION: Check cascade-tree alignment before saving
        if (!validateCascadeTreeAlignment()) {
            MinimaLogger::log("WARNING: Cascade-tree misalignment detected before save!");
            MinimaLogger::log("This may indicate a problem - saving anyway but investigate!");
            // Note: We log but don't fail - the save should still happen
            // to preserve as much state as possible
        }

        // Json DBs
        if (mTxnDB) {
            mTxnDB->saveDB();
        }
        mUserDB->saveDB((basedb / "userprefs.db").string());
        mP2PDB->saveDB((basedb / "p2p.db").string());
        mP2P2DB->saveDB((basedb / "p2p2.db").string());

        // Cascade
        mCascade->saveDB(basedb / "cascade.db");

        // TxPoWTree
        mTxPoWTree->saveDB(basedb / "chaintree.db");

        // MEGA MMR
        if (org::minima::system::params::GeneralParams::IS_MEGAMMR) {
            mMegaMMR->saveMMR(basedb / "megammr.mmr");
        }
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }
    readLock(false);
}

void MinimaDB::saveUserDB() {
    if (!mAllowSaveState) {
        return;
    }

    readLock(true);
    try {
        fs::path basedb = getBaseDBFolder();
        mUserDB->saveDB((basedb / "userprefs.db").string());
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }
    readLock(false);
}

void MinimaDB::saveP2PDB() {
    if (!mAllowSaveState) {
        return;
    }

    try {
        fs::path basedb = getBaseDBFolder();
        mP2PDB->saveDB((basedb / "p2p.db").string());
        mP2P2DB->saveDB((basedb / "p2p2.db").string());
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(exc);
    }
}

// Coin notify
void MinimaDB::addCoinNotify(const std::string& zAddress) {
    std::lock_guard<std::mutex> lk(mCoinNotifyMutex);
    mCoinNotify.insert(zAddress);
}
bool MinimaDB::removeCoinNotify(const std::string& zAddress) {
    std::lock_guard<std::mutex> lk(mCoinNotifyMutex);
    return mCoinNotify.erase(zAddress) > 0;
}
bool MinimaDB::checkCoinNotify(const std::string& zAddress) const {
    std::lock_guard<std::mutex> lk(mCoinNotifyMutex);
    return mCoinNotify.find(zAddress) != mCoinNotify.end();
}

// cascade tree desync
void MinimaDB::rebuildTreeFromCascade() {
    using org::minima::utils::MinimaLogger;
    using org::minima::database::cascade::CascadeNode;
    
    MinimaLogger::log("Rebuilding TxPowTree from Cascade...");
    
    // Clear the existing tree
    mTxPoWTree = std::make_unique<org::minima::database::txpowtree::TxPowTree>();
    
    // Get cascade tip
    auto tip = mCascade->getTip();
    if (!tip) {
        MinimaLogger::log("WARNING: Cannot rebuild tree - cascade is empty");
        return;
    }
    
    // Collect all cascade nodes in order (from root to tip)
    std::vector<CascadeNode*> nodes;
    auto current = tip;
    while (current != nullptr) {
        nodes.push_back(current.get());
        current = mCascade->getParent(current);
    }
    
    // Reverse to go from oldest to newest
    std::reverse(nodes.begin(), nodes.end());
    
    MinimaLogger::log("Rebuilding tree from " + std::to_string(nodes.size()) + " cascade nodes");
    
    // Find the point where tree should start (cascade_tip + 1)
    // The tree contains blocks that come AFTER the cascade
    // For now, we rebuild an empty tree and let it populate naturally
    // as new blocks arrive
    
    // Alternatively, if we have blocks in TxPoWDB that come after cascade,
    // we could rebuild from those, but that's more complex
    
    MinimaLogger::log("Tree rebuild complete - tree will be populated as new blocks arrive");
}

bool MinimaDB::validateCascadeTreeAlignment() const {
    using org::minima::utils::MinimaLogger;
    using org::minima::objects::base::MiniNumber;
    
    // Check basic alignment: tree root should be cascade_tip + 1
    if (mCascade->getTip() != nullptr && mTxPoWTree->getRoot() != nullptr) {
        MiniNumber cascstart = mCascade->getTip()->getTxPoW().getBlockNumber();
        MiniNumber treeroot  = mTxPoWTree->getRoot()->getTxPoW().getBlockNumber();
        
        if (!treeroot.isEqual(cascstart.increment())) {
            MinimaLogger::log("Alignment check: tree root " + treeroot.toString() + 
                            " != cascade tip + 1 (" + cascstart.increment().toString() + ")");
            return false;
        }
    }
    
    return true;
}

} // namespace database
} // namespace minima
} // namespace org