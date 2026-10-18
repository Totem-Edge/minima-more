#include "org/minima/system/main.hpp"

#include <chrono>
#include <thread>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <stdio.h>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/pulse.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/brains/tx_po_w_processor.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/backup/mysql.hpp"
#include "org/minima/system/commands/sendpoll/send_poll_manager.hpp"
#include "org/minima/system/genesis/genesis_m_m_r.hpp"
#include "org/minima/system/genesis/genesis_tx_po_w.hpp"
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"
#include "org/minima/system/network/webhooks/notify_manager.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/message_listener.hpp"
#include "org/minima/utils/messages/timer_message.hpp"
#include "org/minima/utils/messages/timer_processor.hpp"
#include "org/minima/utils/mysql/my_s_q_l_connect.hpp"
#include "org/minima/utils/ssl/s_s_l_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_traffic.hpp"

#include "org/minima/database/userprefs/user_d_b.hpp"

// Additional include for ScriptRow used in doGenesis (method calls)
#include "org/minima/database/wallet/script_row.hpp"

// Concrete headers for classes whose member functions are used
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

using org::minima::database::MinimaDB;
using org::minima::objects::Pulse;
using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::brains::TxPoWMiner;
using org::minima::system::brains::TxPoWProcessor;
using org::minima::system::commands::CommandRunner;
using org::minima::system::commands::backup::mysql;
using org::minima::system::commands::sendpoll::SendPollManager;
using org::minima::system::genesis::GenesisMMR;
using org::minima::system::genesis::GenesisTxPoW;
using org::minima::system::network::NetworkManager;
using org::minima::system::network::minima::NIOManager;
using org::minima::system::network::minima::NIOMessage;
using org::minima::system::network::p2p::P2PFunctions;
using org::minima::system::network::webhooks::NotifyManager;
using org::minima::system::params::GeneralParams;
using org::minima::system::params::GlobalParams;
using org::minima::utils::MiniFile;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::utils::messages::Message;
using org::minima::utils::messages::MessageListener;
using org::minima::utils::messages::TimerMessage;
using org::minima::utils::messages::TimerProcessor;
using org::minima::utils::mysql::MySQLConnect;

namespace org {
namespace minima {
namespace system {

// Static definitions
bool Main::STARTUP_DEBUG_LOGS = false;

Main* Main::sMainInstance = nullptr;
MessageListener* Main::sMinimaListener = nullptr;

const std::string Main::MAIN_TXPOWMINED           = "MAIN_TXPOWMINED";
const std::string Main::MAIN_PULSE                = "MAIN_PULSE";
const std::string Main::MAIN_CLEANDB_RAM          = "MAIN_CLEANDB_RAM";
const std::string Main::MAIN_CLEANDB_SQL          = "MAIN_CLEANDB_SQL";
const std::string Main::MAIN_SYSTEMCLEAN          = "MAIN_SYSTEMCLEAN";
const std::string Main::MAIN_AUTOBACKUP_MYSQL     = "MAIN_AUTOBACKUP_MYSQL";
const std::string Main::MAIN_AUTOBACKUP_TXPOW     = "MAIN_AUTOBACKUP_TXPOW";
const std::string Main::MAIN_DO_RESCUE            = "MAIN_DO_RESCUE";
const std::string Main::MAIN_AUTOBACKUP           = "MAIN_AUTOBACKUP";
const std::string Main::MAIN_SHUTDOWN             = "MAIN_SHUTDOWN";
const std::string Main::MAIN_NETRESTART           = "MAIN_NETRESTART";
const std::string Main::MAIN_NETRESET             = "MAIN_NETRESET";
const std::string Main::MAIN_CHECKER              = "MAIN_CHECKER";
const std::string Main::MAIN_INIT_KEYS            = "MAIN_INIT_KEYS";
const std::string Main::MAIN_CALLCHECKER          = "MAIN_CALLCHECKER";
const std::string Main::MAIN_P2PNET_CHECKER    = "MAIN_P2PNET_CHECKER";
const std::string Main::MAIN_NEWBLOCK             = "MAIN_NEWBLOCK";
const std::string Main::MAIN_BALANCE              = "MAIN_BALANCE";
const std::string Main::MAIN_MINING               = "MAIN_MINING";
const std::string Main::MAIN_NEWCOIN              = "NEWCOIN";
const std::string Main::MAIN_NOTIFYCOIN           = "NOTIFYCOIN";
const std::string Main::MAIN_NOTIFYCASCADEBLOCK   = "NOTIFYCASCADEBLOCK";
const std::string Main::MAIN_NOTIFYCASCADETXN     = "NOTIFYCASCADETXN";
const std::string Main::MAIN_NOTIFYCASCADECOIN    = "NOTIFYCASCADECOIN";

Main* Main::getInstance() {
    return sMainInstance;
}

void Main::ClearMainInstance() {
    if (sMainInstance != nullptr) {
        sMainInstance = nullptr;
        MinimaLogger::log("Main Instance Cleared..");
    }
}

MessageListener* Main::getMinimaListener() {
    return sMinimaListener;
}

void Main::setMinimaListener(MessageListener* zListener) {
    sMinimaListener = zListener;
}

Main::Main() : MessageProcessor("MAIN") {

    if (STARTUP_DEBUG_LOGS) {
        MinimaLogger::log("MAIN init.. start");
    }

    // Start uptime
    mUptimeMilli = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    // Set static instance
    sMainInstance = this;


    // Create Timer Processor
    TimerProcessor::createTimerProcessor();

    // PRIVATE network first-run detection
    if (GeneralParams::PRIVATE_) {
        std::filesystem::path basefolder = std::filesystem::path(GeneralParams::DATA_FOLDER) / "databases";
        std::filesystem::path userdb = basefolder / "userprefs.db";
        if (std::filesystem::exists(userdb) && !GeneralParams::CLEAN) {
            MinimaLogger::log("SOLO NETWORK : userdb found : not first run.. no -genesis..");
        } else {
            MinimaLogger::log("SOLO NETWORK : userdb not found : FIRST RUN.. creating genesis coins..");
            GeneralParams::CLEAN = true;
            GeneralParams::GENESIS = true;
        }
    }

    // Are we deleting previous..
    if (GeneralParams::CLEAN) {
        MinimaLogger::log("Wiping previous config files..");
        // Delete the conf folder
        MiniFile::deleteFileOrFolder(GeneralParams::DATA_FOLDER, std::filesystem::path(GeneralParams::DATA_FOLDER));
    }

    // Create the MinimaDB
    if (STARTUP_DEBUG_LOGS) {
        MinimaLogger::log("MinimaDB create.. start");
    }

    MinimaDB::createDB();
    if (STARTUP_DEBUG_LOGS) {
        MinimaLogger::log("MinimaDB create.. finish");
    }

    // Load the Databases
    if (STARTUP_DEBUG_LOGS) {
        MinimaLogger::log("Load all DB.. start");
    }

    MinimaDB::getDB()->loadAllDB();
    if (STARTUP_DEBUG_LOGS) {
        MinimaLogger::log("Load all DB.. finish");
    }

    // Auto MySQL details?
    if (!GeneralParams::MYSQL_DB_DETAILS.empty()) {
        mysql::convertMySQLParams(GeneralParams::MYSQL_DB_DETAILS);
    }

    // Slave node mode
    bool slavemode = MinimaDB::getDB()->getUserDB().isSlaveNode();
    if (slavemode) {
        GeneralParams::CONNECT_LIST = MinimaDB::getDB()->getUserDB().getSlaveNodeHost();
        GeneralParams::P2P_ENABLED = false;
        GeneralParams::TXBLOCK_NODE = true;
        GeneralParams::NO_SYNC_IBD = true;
        GeneralParams::IS_ACCEPTING_IN_LINKS = false;
        MinimaLogger::log(std::string("Slave Mode ENABLED master:") + GeneralParams::CONNECT_LIST);
    }

    // Create SSL keystore
    if (STARTUP_DEBUG_LOGS) {
        MinimaLogger::log("SSL Key.. start");
    }

    org::minima::utils::ssl::SSLManager::makeKeyFile();
    if (STARTUP_DEBUG_LOGS) {
        MinimaLogger::log("SSL Key.. finish");
    }

    // Init hash rate
    TxPoWMiner::calculateHashRateOld(MiniNumber(10000));

    // Delete archive restore folder
    {
        std::filesystem::path restorefolder = std::filesystem::path(GeneralParams::DATA_FOLDER) / "archiverestore";
        MiniFile::deleteFileOrFolder(GeneralParams::DATA_FOLDER, restorefolder);
    }

    // Calculate hash speed, store
    {
        MiniNumber hashcheck("250000");
        MiniNumber hashrate = TxPoWMiner::calculateHashSpeed(hashcheck);
        MinimaDB::getDB()->getUserDB().setHashRate(hashrate);
        MinimaLogger::log(std::string("Calculate device hash rate : ")
            + hashrate.div(MiniNumber::MILLION()).setSignificantDigits(4).toString() + " MHs");
    }

    // Create initial key set
    try {
        mInitKeysCreated = MinimaDB::getDB()->getWallet().initDefaultKeys(3);
    } catch (const std::exception& exc) {
        MinimaLogger::log(exc);
    }

    // Notify manager
    mNotifyManager = std::make_unique<NotifyManager>();

    // Start the engine
    mTxPoWProcessor = std::make_unique<TxPoWProcessor>();
    mTxPoWMiner     = std::make_unique<TxPoWMiner>();

    // Genesis?
    if (GeneralParams::GENESIS) {
        doGenesis();
    }

    // Clear invalid peers
    P2PFunctions::clearInvalidPeers();

    // Start networking
    mNetwork = std::make_unique<NetworkManager>();

    // SendPoll Manager
    mSendPoll = std::make_unique<SendPollManager>();

    // Simulate traffic message: auto-mine pulse schedule
    AUTOMINE_TIMER = MiniNumber::THOUSAND().div(GlobalParams::MINIMA_BLOCK_SPEED).getAsLong();
    mTxPoWMiner->PostTimerMessage(std::make_shared<TimerMessage>(AUTOMINE_TIMER, TxPoWMiner::TXPOWMINER_MINEPULSE));

    // PULSE message timer
    PostTimerMessage(std::make_shared<TimerMessage>(org::minima::system::params::GeneralParams::USER_PULSE_FREQ, MAIN_PULSE));

    // Clean DB timers
    if (GeneralParams::GENESIS) {
        PostTimerMessage(std::make_shared<TimerMessage>(10 * 1000, MAIN_CLEANDB_RAM));
    } else {
        PostTimerMessage(std::make_shared<TimerMessage>(3 * 60 * 1000, MAIN_CLEANDB_RAM));
    }
    PostTimerMessage(std::make_shared<TimerMessage>(10 * 60 * 1000, MAIN_CLEANDB_SQL));

    // System clean
    PostTimerMessage(std::make_shared<TimerMessage>(SYSTEMCLEAN_TIMER, MAIN_SYSTEMCLEAN));

    // Debug checker
    PostTimerMessage(std::make_shared<TimerMessage>(CHECKER_TIMER, MAIN_CHECKER));

    // Init keys check
    PostTimerMessage(std::make_shared<TimerMessage>(1000 * 30, MAIN_INIT_KEYS));

    // Network reset timer
    PostTimerMessage(std::make_shared<TimerMessage>(NETRESET_TIMER, MAIN_NETRESET));

    // AutoBackup timers
    PostTimerMessage(std::make_shared<TimerMessage>(1000 * 60 * 5, MAIN_AUTOBACKUP));
    PostTimerMessage(std::make_shared<TimerMessage>(MAIN_AUTOBACKUP_MYSQL_TIMER, MAIN_AUTOBACKUP_MYSQL));

    // P2P checker
    PostTimerMessage(std::make_shared<TimerMessage>(P2PNET_TIMER, MAIN_P2PNET_CHECKER));

    // Check slave mode connect list
    if (GeneralParams::TXBLOCK_NODE) {
        if (GeneralParams::CONNECT_LIST.find(',') != std::string::npos) {
            MinimaLogger::log("[!] Can ONLY connect to 1 host in slave mode.. stopping");
            std::exit(1);
        }
        MinimaLogger::log(std::string("Running in slave mode. Will Connect to ") + GeneralParams::CONNECT_LIST);
    }

    // Initialize old tip pointer
    mOldTip = MiniData(MiniData::ZERO_TXPOWID());

    startMessageProcessorThread();
}

Main::~Main() {

}

void Main::setSyncIBD(bool zSync) {
    if (org::minima::system::params::GeneralParams::IBDSYNC_LOGS) {
        MinimaLogger::log(std::string("SYNC IBD LOCK : ") + (zSync ? "true" : "false"));
    }
    mSyncIBD = zSync;
}

bool Main::isSyncIBD() const {
    return mSyncIBD;
}

void Main::setHasShutDown() {
    mShuttingdown = true;
}

bool Main::isShuttingDown() const {
    return mShuttingdown;
}

bool Main::isRestoring() const {
    return mRestoring;
}

bool Main::isShuttongDownOrRestoring() const {
    return mShuttingdown || mRestoring;
}

void Main::shutdown() {
    shutdown(false);
}

void Main::shutdown(bool zCompact) {
    if (mShuttingdown) {
        MinimaLogger::log("Shutdown called when already shutting down..");
        return;
    }

    if (zCompact) {
        MinimaLogger::log("Shut down started.. Compacting All Databases");
    } else {
        MinimaLogger::log("Shut down started..");
    }

    mShuttingdown = true;

    try {
        // Tell wallet
        MinimaDB::getDB()->getWallet().shuttingDown();

        // Shut down network and other generators
        shutdownGenProcs();

        // Stop main TxPoW processor
        shutdownFinalProcs();

        // Save DBs
        MinimaLogger::log("Saving all db");
        MinimaDB::getDB()->saveAllDB(zCompact);

        // Stop this
        stopMessageProcessor();

        // Wait for shutdown
        MinimaLogger::log("Main thread shutdown");
        waitToShutDown();

        MinimaLogger::log("Shut down completed OK..");

        // Notify listener
        NotifyMainListenerOfShutDown();

    } catch (const std::exception& exc) {
        MinimaLogger::log("ERROR Shutting down..");
        MinimaLogger::log(exc);
    }
}

void Main::NotifyMainListenerOfShutDown() {
    // Called from various functions
    ClearMainInstance();

    if (mShutDownSentToListener) {
        return;
    }
    mShutDownSentToListener = true;

    try {
        NotifyMainListenerOnly("SHUTDOWN");
    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    }
}

void Main::setStartUpError(bool zStartError, const std::string& zMessage) {
    STARTUP_ERROR = zStartError;
    STARTUP_ERROR_MSG = zMessage;
}

bool Main::isStartupError() const {
    return STARTUP_ERROR;
}

std::string Main::getStartupErrorMsg() const {
    return STARTUP_ERROR_MSG;
}

void Main::restoreReady() {
    restoreReady(true);
}

void Main::restoreReady(bool zShutdownFinalProcs) {
    mRestoring = true;

    // Shut down the network
    shutdownGenProcs();

    // Stop the main TxPoW processor
    shutdownFinalProcs(zShutdownFinalProcs);
}

void Main::restoreReadyForSync() {
    // Restart the Processor
    mTxPoWProcessor = std::make_unique<TxPoWProcessor>();

    // Reload the DBs
    MinimaDB::getDB()->loadDBsForRestoreSync();
}

void Main::archiveResetReady(bool zResetWallet) {
    archiveResetReady(zResetWallet, true);
}

void Main::archiveResetReady(bool zResetWallet, bool zResetCascadeTree) {
    mRestoring = true;

    // Shut most of the processors down
    shutdownGenProcs();

    std::error_code ec;
    
    // Save/Delete TxPoWDB
    MinimaDB::getDB()->getTxPoWDB().getSQLDB()->saveDB(false);
    if (zResetCascadeTree) {
        std::filesystem::path txpowsqlfile = MinimaDB::getDB()->getTxPoWDB().getSQLDB()->getSQLFile();
        if (std::filesystem::exists(txpowsqlfile)) {
            std::filesystem::remove(txpowsqlfile, ec);
            if(ec) { MinimaLogger::log("Error deleting txpowsqlfile: " + ec.message()); }
        }
    }
    
    // Save/Delete ArchiveDB
    MinimaDB::getDB()->getArchive().saveDB(false);
    std::filesystem::path archivefile = MinimaDB::getDB()->getArchive().getSQLFile();
    if (std::filesystem::exists(archivefile)) {
        std::filesystem::remove(archivefile, ec);
        if(ec) { MinimaLogger::log("Error deleting archivefile: " + ec.message()); }
    }
    
    // Save/Delete WalletDB
    if (zResetWallet) {
        MinimaDB::getDB()->getWallet().saveDB(false);
        std::filesystem::path walletfile = MinimaDB::getDB()->getWallet().getSQLFile();
        if (std::filesystem::exists(walletfile)) {
            std::filesystem::remove(walletfile, ec);
            if(ec) { MinimaLogger::log("Error deleting walletfile: " + ec.message()); }
        }
    }

    // Reload the SQL dbs (reset wallet if requested)
    MinimaDB::getDB()->loadArchiveAndTxPoWDB(zResetWallet);

    if (zResetCascadeTree) {
        // Reset Cascade and TxPoWTree
        MinimaDB::getDB()->resetCascadeAndTxPoWTree();

        // Delete the cascade file
        MinimaLogger::log("Deleting cascade..");
        std::filesystem::path cdb = MinimaDB::getDB()->getCascadeFile();
        if (std::filesystem::exists(cdb)) {
            std::error_code ec;
            std::filesystem::remove(cdb, ec);
        }
    }
}

void Main::shutdownGenProcs() {
    // No more timer Messages
    TimerProcessor::stopTimerProcessor();

    // Shut down the network
    if (mNetwork) {
        mNetwork->shutdownNetwork();
    }

    // Stop Miner
    if (mTxPoWMiner) {
        mTxPoWMiner->stopMessageProcessor();
    }

    // Stop sendPoll
    if (mSendPoll) {
        mSendPoll->stopMessageProcessor();
    }

    // Wait for networking to finish
    long long timewaited = 0;
    while (mNetwork && !mNetwork->isShutDownComplete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        timewaited += 250;
        if (timewaited > 10000) {
            MinimaLogger::log("Network shutdown took too long..");
            mNetwork->hardShutDown();
            break;
        }
    }
}

void Main::shutdownFinalProcs() {
    shutdownFinalProcs(true);
}

void Main::shutdownFinalProcs(bool zShutDownFinalProcs) {
    if (zShutDownFinalProcs) {
        if (!mHaveShutDownFinalProcs) {
            mHaveShutDownFinalProcs = true;
            if (mNotifyManager) {
                mNotifyManager->shutDown();
            }
        }
    }

    // Stop the main TxPoW processor
    MinimaLogger::log("Shutdown TxPoWProcessor..");
    if (mTxPoWProcessor) {
        mTxPoWProcessor->stopMessageProcessor();
        mTxPoWProcessor->waitToShutDown();
    }
}

void Main::resetMemFull() {
    // Reset all DBs
    MinimaDB::getDB()->fullDBRestartMemFree();

    if (mTxPoWProcessor) {
        mTxPoWProcessor->stopMessageProcessor();
        mTxPoWProcessor->waitToShutDown();
    }

    // Reset main processor
    mTxPoWProcessor = std::make_unique<TxPoWProcessor>();
}

void Main::restartNIO() {
    if (mShuttingdown) {
        return;
    }

    MinimaDB::getDB()->readLock(true);
    try {
        MinimaLogger::log("Network Shutdown started..");

        if (mNetwork) {
            mNetwork->shutdownNetwork();
        }

        long long timewaited = 0;
        while (mNetwork && !mNetwork->isShutDownComplete()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            timewaited += 250;

            if (timewaited > 10000) {
                mNetwork->hardShutDown();
                break;
            }
        }

        MinimaLogger::log("Network Shutdown complete.. restart in 5 seconds");
        std::this_thread::sleep_for(std::chrono::seconds(5));

        mNetwork = std::make_unique<NetworkManager>();

        MinimaLogger::log("Network restarted..");
    } catch (const std::exception&) {
        MinimaLogger::log("[!] Error restarting Network.. Restart Minima!");
    }
    // Ensure unlock regardless of exceptions
    MinimaDB::getDB()->readLock(false);
}

void Main::setNormalAutoMineSpeed() {
    mNormalMineMode = true;
    AUTOMINE_TIMER = static_cast<long long>(1000) * 50;
}

void Main::setLowPowAutoMineSpeed() {
    mNormalMineMode = false;
    AUTOMINE_TIMER = static_cast<long long>(1000) * 500;
}

bool Main::isNormalMineMode() const {
    return mNormalMineMode;
}

long long Main::getUptimeMilli() const {
    long long now = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return now - mUptimeMilli;
}

NetworkManager& Main::getNetworkManager() {
    return *mNetwork;
}

NIOManager& Main::getNIOManager() {
    return mNetwork->getNIOManager();
}

NotifyManager& Main::getNotifyManager() {
    return *mNotifyManager;
}

TxPoWProcessor& Main::getTxPoWProcessor() {
    return *mTxPoWProcessor;
}

TxPoWMiner& Main::getTxPoWMiner() {
    return *mTxPoWMiner;
}

SendPollManager& Main::getSendPoll() {
    return *mSendPoll;
}

void Main::doGenesis() {
    // Create a new address - to receive the genesis funds..
    auto scrow = MinimaDB::getDB()->getWallet().createNewSimpleAddress(true);

    // Create Genesis TxPoW as a shared_ptr
    auto genesis_ptr = std::make_shared<GenesisTxPoW>(scrow->getAddress());

    // Hard add to DB
    MinimaDB::getDB()->getTxPoWDB().addTxPoW(genesis_ptr); // FIX: Pass the shared_ptr

    // Create Genesis TxBlock
    GenesisMMR gmmr;
    std::vector<org::minima::objects::TxPoW*> empty_txns;
    TxBlock txgenesisblock(gmmr, *genesis_ptr, empty_txns);

    // First root node as shared_ptr
    auto root = std::make_shared<org::minima::database::txpowtree::TxPoWTreeNode>(txgenesisblock);
    // MinimaLogger::log(std::string("DEBUG: doGenesis() created root node: ") + root->getTxPoW().getTxPoWID());

    // Set it
    MinimaDB::getDB()->getTxPoWTree().setRoot(root);
    // MinimaLogger::log("DEBUG: doGenesis() TxPoWTree root set.");

    // DEGUB: IMMEDIATELY check if the tip is valid
    // auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
    // if (!tip) {
    //     MinimaLogger::log("DEBUG: doGenesis() FAILED. Tip is still null after setRoot()."); 
    // } else {
    //     MinimaLogger::log(std::string("DEBUG: doGenesis() SUCCESS. Tip is now: ") + tip->getTxPoW().getTxPoWID()); 
    // }

    // Set as main chain
    MinimaDB::getDB()->getTxPoWDB().setOnMainChain(genesis_ptr->getTxPoWID()); 
}

bool Main::getAllKeysCreated() const {
    return mInitKeysCreated;
}

int Main::getAllDefaultKeysSize() const {
    return MinimaDB::getDB()->getWallet().getDefaultKeysNumber();
}

void Main::processMessage(Message& zMessage) {
    if (mShuttingdown || mRestoring) {
        return;
    }

    const std::string& mtype = zMessage.getMessageType();

    if (mtype == MAIN_TXPOWMINED) {
        // Get it
        auto anytxp = zMessage.getObject("txpow");
        TxPoW* txpow = anyToTxPoWPtr(anytxp);
        if (!txpow) {
            return;
        }

        if (!txpow->isTransaction() && !txpow->isBlock()) {
            return;
        }

        if (!GeneralParams::TEST_PARAMS && txpow->isBlock()) {
            MinimaLogger::log(std::string("You found a block! ") + txpow->getBlockNumber().toString() + " " + txpow->getTxPoWID());
        }

        // Create NIO message
        MiniData niodata = NIOManager::createNIOMessage(NIOMessage::MSG_TXPOW(), *txpow);

        // And send
        auto newniomsg = std::make_shared<Message>(NIOManager::NIO_INCOMINGMSG); // FIX: Create as shared_ptr
        newniomsg->addString("uid", "0x00");                                    // FIX: Use ->
        newniomsg->addObject("data", niodata);                                  // FIX: Use ->

        // Post to the NIOManager
        getNetworkManager().getNIOManager().PostMessage(newniomsg); // FIX: Pass shared_ptr

    } else if (mtype == MAIN_SYSTEMCLEAN) {
        PostTimerMessage(std::make_shared<TimerMessage>(SYSTEMCLEAN_TIMER, MAIN_SYSTEMCLEAN));
        // System.gc(); no-op
        org::minima::utils::MinimaLogger::log("Running system clean cycle..");

    } else if (mtype == MAIN_CLEANDB_RAM) {
        PostTimerMessage(std::make_shared<TimerMessage>(CLEANDB_RAM_TIMER, MAIN_CLEANDB_RAM));

        MinimaDB::getDB()->getTxPoWDB().cleanDBRAM();
        MinimaDB::getDB()->saveState();
        MinimaDB::getDB()->refreshSQLDB();

    } else if (mtype == MAIN_AUTOBACKUP_MYSQL) {
        auto& udb = MinimaDB::getDB()->getUserDB();

        if (MYSQL_IMPORTING_NO_ACTION) {
            if (udb.getAutoBackupMySQL()) {
                MinimaLogger::log("Skipping MySQL Backup as importing data already..");
            }
            PostTimerMessage(std::make_shared<TimerMessage>(MAIN_AUTOBACKUP_MYSQL_TIMER, MAIN_AUTOBACKUP_MYSQL));
            return;
        }

        try {
            if (udb.getAutoBackupMySQL()) {
                std::string backupcommand = std::string("mysql host:") + udb.getAutoMySQLHost()
                    + " database:" + udb.getAutoMySQLDB()
                    + " user:" + udb.getAutoMySQLUser()
                    + " password:" + udb.getAutoMySQLPassword()
                    + " action:update";

                auto runner = CommandRunner::getRunner();
                auto res = runner->runMultiCommand(backupcommand);
                if (res) {
                    MinimaLogger::log(std::string("MYSQL AUTOBACKUP result ") + res->toString());
                }
            }

            if (udb.getAutoBackupMySQLCoins()) {
                std::string backupcommand = std::string("mysqlcoins host:") + udb.getAutoMySQLHost()
                    + " database:" + udb.getAutoMySQLDB()
                    + " user:" + udb.getAutoMySQLUser()
                    + " password:" + udb.getAutoMySQLPassword()
                    + " action:update";

                auto runner = CommandRunner::getRunner();
                auto res = runner->runMultiCommand(backupcommand);
                if (res) {
                    MinimaLogger::log(std::string("MYSQLCOINS AUTOBACKUP result ") + res->toString());
                }
            }
        } catch (const std::exception& exc) {
            MinimaLogger::log(exc);
        }

        PostTimerMessage(std::make_shared<TimerMessage>(MAIN_AUTOBACKUP_MYSQL_TIMER, MAIN_AUTOBACKUP_MYSQL));

    } else if (mtype == MAIN_AUTOBACKUP_TXPOW) {
        if (MYSQL_IMPORTING_NO_ACTION) {
            return;
        }

        if (GeneralParams::MYSQL_STORE_ALLTXPOW) {
            auto& udb = MinimaDB::getDB()->getUserDB();

            if (udb.getAutoBackupMySQL()) {
                auto anytxp = zMessage.getObject("txpow");
                TxPoW* txp = anyToTxPoWPtr(anytxp);
                if (!txp) {
                    return;
                }

                MySQLConnect mysqlc(udb.getAutoMySQLHost(),
                                    udb.getAutoMySQLDB(),
                                    udb.getAutoMySQLUser(),
                                    udb.getAutoMySQLPassword());
                try {
                    mysqlc.init();
                    bool status = mysqlc.saveTxPoW(*txp);
                    mysqlc.shutdown();

                    if (!status) {
                        MinimaLogger::log(std::string("[ERROR] MYSQL TXPOW AUTOBACKUP ")
                            + " host:" + udb.getAutoMySQLHost()
                            + " user:" + udb.getAutoMySQLUser()
                            + " db:" + udb.getAutoMySQLDB());
                    }
                } catch (const std::exception& exc) {
                    MinimaLogger::log(exc);
                }
            }
        }

    } else if (mtype == MAIN_CLEANDB_SQL) {
        PostTimerMessage(std::make_shared<TimerMessage>(CLEANDB_SQL_TIMER, MAIN_CLEANDB_SQL));

        MinimaDB::getDB()->getTxPoWDB().cleanDBSQL();
        MinimaDB::getDB()->getArchive().checkForCleanDB();

    } else if (mtype == MAIN_DO_RESCUE) {
        if (!GeneralParams::RESCUE_MEGAMMR_NODE.empty()) {
            MinimaLogger::log(std::string("Running MegaMMR Sync from Rescuse Node ") + GeneralParams::RESCUE_MEGAMMR_NODE);

            mInitKeysCreated = true;

            std::string command = std::string("megammrsync action:resync host:") + GeneralParams::RESCUE_MEGAMMR_NODE;

            auto runner = CommandRunner::getRunner();
            auto res = runner->runSingleCommand(command);
            if (res) {
                MinimaLogger::log(res->toString());
            }

            std::_Exit(0);
        }

    } else if (mtype == MAIN_PULSE) {
        PostTimerMessage(std::make_shared<TimerMessage>(org::minima::system::params::GeneralParams::USER_PULSE_FREQ, MAIN_PULSE));

        if (GeneralParams::TXBLOCK_NODE) {
            return;
        }

        Pulse pulse = Pulse::createPulse();
        NIOManager::sendNetworkMessageAll(NIOMessage::MSG_PULSE(), pulse);

    } else if (mtype == MAIN_NEWBLOCK) {
        auto anytxp = zMessage.getObject("txpow");
        TxPoW* txpow = anyToTxPoWPtr(anytxp);
        if (!txpow) {
            return;
        }

        JSONObject data;
        data.put("txpow", txpow->toJSON());
        PostNotifyEvent("NEWBLOCK", data);

    } else if (mtype == MAIN_BALANCE) {

        JSONObject data;
        PostNotifyEvent("NEWBALANCE", data);

    } else if (mtype == MAIN_MINING) {
        auto anytxp = zMessage.getObject("txpow");
        TxPoW* txpow = anyToTxPoWPtr(anytxp);
        if (!txpow) {
            return;
        }

        bool starting = zMessage.getBoolean("starting");

        JSONObject data;
        data.put("txpow", txpow->toJSON());
        data.put("mining", starting);

        PostNotifyEvent("MINING", data);

    } else if (mtype == MAIN_NETRESTART) {
        MinimaLogger::log("[!] MAIN restart networking..");

        MinimaLogger::log("Disconnect all peers");
        Main::getInstance()->getNetworkManager().getNIOManager().PostMessage(NIOManager::NIO_DISCONNECTALL);

        MinimaLogger::log("Wait 10 seconds..");
        std::this_thread::sleep_for(std::chrono::seconds(10));

        getTxPoWProcessor().resetFirstIBDTimer();
        P2PFunctions::clearInvalidPeers();
        NIOMessage::mHaveSentIBDRecently.clear();

        restartNIO();

    } else if (mtype == MAIN_AUTOBACKUP) {
        PostTimerMessage(std::make_shared<TimerMessage>(AUTOBACKUP_TIMER, MAIN_AUTOBACKUP));

        if (MinimaDB::getDB()->getUserDB().isAutoBackup()) {
            auto runner = CommandRunner::getRunner();
            auto res = runner->runMultiCommand("backup");
            if (res) {
                MinimaLogger::log(std::string("AUTOBACKUP : ") + res->toString());
            }
        }

        MiniNumber hashcheck("250000");
        MiniNumber hashrate = TxPoWMiner::calculateHashSpeed(hashcheck);
        MinimaDB::getDB()->getUserDB().setHashRate(hashrate);
        MinimaLogger::log(std::string("Re-Calculate device hash rate : ")
            + hashrate.div(MiniNumber::MILLION()).setSignificantDigits(4).toString() + " MHs");

    } else if (mtype == MAIN_NETRESET) {
        // Reset the networking stats
        Main::getInstance()->getNIOManager().getTrafficListener().reset();

        // Reset Network stats every 24 hours
        PostTimerMessage(std::make_shared<TimerMessage>(NETRESET_TIMER, MAIN_NETRESET));

    } else if (mtype == MAIN_SHUTDOWN) {
        shutdown();

    } else if (mtype == MAIN_INIT_KEYS) {
        if (!mInitKeysCreated) {
            try {
                mInitKeysCreated = MinimaDB::getDB()->getWallet().initDefaultKeys(8);
                if (mInitKeysCreated) {
                    MinimaLogger::log("All default getaddress keys created..");
                }
            } catch (const std::exception& exc) {
                MinimaLogger::log(exc);
            }
        }

        if (!mInitKeysCreated) {
            PostTimerMessage(std::make_shared<TimerMessage>(INIT_KEYS_TIMER, MAIN_INIT_KEYS));
        }

    } else if (mtype == MAIN_CHECKER) {
        PostTimerMessage(std::make_shared<TimerMessage>(CHECKER_TIMER, MAIN_CHECKER));

        auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
        if (!tip) {
            MinimaLogger::log("No tip found in Main Checker..");
            return;
        }

        if (tip->getTxPoW().getTxPoWIDData().isEqual(mOldTip)) {
            MinimaLogger::log(std::string("Warning : Chain tip hasn't changed in 180 seconds ")
                + tip->getTxPoW().getTxPoWID() + " " + tip->getTxPoW().getBlockNumber().toString());
        }

        mOldTip = tip->getTxPoW().getTxPoWIDData();

        MiniData tipid = tip->getTxPoW().getTxPoWIDData(); // FIX: Store rvalue in a local variable
        NIOManager::sendNetworkMessageAll(NIOMessage::MSG_PING(), tipid); // FIX: Pass the lvalue

    } else if (mtype == MAIN_P2PNET_CHECKER) {
        PostTimerMessage(std::make_shared<TimerMessage>(P2PNET_TIMER, MAIN_P2PNET_CHECKER));

        bool restartsent = false;
        if (GeneralParams::P2P_ENABLED && P2PFunctions::isNetAvailable()) {
            long long timenow = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

            auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
            if (!tip) {
                return;
            }

            long long tiptime = tip->getTxPoW().getTimeMilli().getAsLong();
            long long diff = timenow - tiptime;

            if (diff > 1000LL * 60 * 120) {
                MinimaLogger::log("[!] Chain Tip too far behind.. restart Networking!");
                restartsent = true;
                Main::getInstance()->PostMessage(MAIN_NETRESTART);
            }
        }

        if (GeneralParams::P2P_ENABLED) {
            int count = Main::getInstance()->getNetworkManager().getP2PManager().getSize();
            if (count > 50) {
                MinimaLogger::log("[!] P2P Message Overload - Restart");
                if (!restartsent) {
                    restartsent = true;
                    Main::getInstance()->PostMessage(MAIN_NETRESTART);
                }
            }
        }

        P2PFunctions::clearInvalidPeers();
        NIOMessage::mHaveSentIBDRecently.clear();

    } else if (mtype == MAIN_CALLCHECKER) {
        bool timed = zMessage.getBoolean("timer", false);
        MinimaLogger::log(std::string("MAIN Checker Call Recieved.. timer:") + (timed ? "true" : "false"));
    }
}

void Main::PostNotifyEvent(const std::string& zEvent, const JSONObject& zData) {
    PostNotifyEvent(zEvent, zData, "*");
}

void Main::PostNotifyEvent(const std::string& zEvent, const JSONObject& zData, const std::string& zTo) {
    JSONObject notify;
    notify.put("event", zEvent);
    notify.put("data", zData);

    if (zTo == "*") {
        if (mNotifyManager) {
            auto sp = std::make_shared<JSONObject>(notify);
            mNotifyManager->PostEvent(sp);
        }
    }
}

void Main::NotifyMainListenerOnly(const std::string& zMessage) {
    if (getMinimaListener() != nullptr) {
        JSONObject notify;
        notify.put("event", zMessage);
        notify.put("data", JSONObject());

        auto msg = std::make_shared<Message>(NotifyManager::NOTIFY_POST);
        msg->addObject("notify", notify);

        getMinimaListener()->processMessage(msg);
    }
}

TxPoW* Main::anyToTxPoWPtr(const std::any& a) {
    if (!a.has_value()) {
        return nullptr;
    }
    try {
        // Try shared_ptr<TxPoW>
        auto sp = std::any_cast<std::shared_ptr<TxPoW>>(a);
        if (sp) return sp.get();
    } catch (...) {}
    try {
        // Try raw pointer
        auto rp = std::any_cast<TxPoW*>(a);
        if (rp) return rp;
    } catch (...) {}
    try {
        // Try reference wrapper
        auto& ref = std::any_cast<std::reference_wrapper<TxPoW>>(const_cast<std::any&>(a)).get();
        return &ref;
    } catch (...) {}
    return nullptr;
}

TxPoW* Main::ensureRaw(TxPoW& ref) {
    return &ref;
}

} // namespace system
} // namespace minima
} // namespace org