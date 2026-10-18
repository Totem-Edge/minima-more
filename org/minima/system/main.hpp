#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <any>

#include "org/minima/utils/messages/message_processor.hpp"
#include "org/minima/objects/base/mini_data.hpp"

// Forward declarations (Pitfall 10)
namespace org { namespace minima { namespace utils { namespace messages { class Message; class MessageListener; } } } }
namespace org { namespace minima { namespace database { class MinimaDB; } } }
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; } } } }
namespace org { namespace minima { namespace database { namespace userprefs { class UserDB; } } } }
namespace org { namespace minima { namespace database { namespace wallet { class Wallet; } } } }
namespace org { namespace minima { namespace system { namespace brains { class TxPoWMiner; class TxPoWProcessor; } } } }
namespace org { namespace minima { namespace system { namespace network { class NetworkManager; } } } }
namespace org { namespace minima { namespace system { namespace network { namespace minima { class NIOManager; } } } } }
namespace org { namespace minima { namespace system { namespace network { namespace webhooks { class NotifyManager; } } } } }
namespace org { namespace minima { namespace system { namespace commands { namespace sendpoll { class SendPollManager; } } } } }
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace objects { class TxBlock; class TxPoW; class Pulse; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace system {

class Main final : public org::minima::utils::messages::MessageProcessor {
public:
    // Static flags
    static bool STARTUP_DEBUG_LOGS;

    // Static access to Main singleton
    static Main* getInstance();
    static void ClearMainInstance();

    // Android-style listener
    static org::minima::utils::messages::MessageListener* getMinimaListener();
    static void setMinimaListener(org::minima::utils::messages::MessageListener* zListener);

    // Public constants (Java public static finals)
    static const std::string MAIN_TXPOWMINED;
    static const std::string MAIN_PULSE;

    static const std::string MAIN_CLEANDB_RAM;
    static const std::string MAIN_CLEANDB_SQL;
    static const std::string MAIN_SYSTEMCLEAN;

    static const std::string MAIN_AUTOBACKUP_MYSQL;
    static const std::string MAIN_AUTOBACKUP_TXPOW;
    static const std::string MAIN_DO_RESCUE;

    static const std::string MAIN_AUTOBACKUP;

    static const std::string MAIN_SHUTDOWN;

    static const std::string MAIN_NETRESTART;
    static const std::string MAIN_NETRESET;

    static const std::string MAIN_CHECKER;
    static const std::string MAIN_INIT_KEYS;

    static const std::string MAIN_CALLCHECKER;
    static const std::string MAIN_P2PNET_CHECKER;

    static const std::string MAIN_NEWBLOCK;
    static const std::string MAIN_BALANCE;
    static const std::string MAIN_MINING;

    static const std::string MAIN_NEWCOIN;
    static const std::string MAIN_NOTIFYCOIN;
    static const std::string MAIN_NOTIFYCASCADEBLOCK;
    static const std::string MAIN_NOTIFYCASCADETXN;
    static const std::string MAIN_NOTIFYCASCADECOIN;

public:
    Main();

    // PIMPL-fix for unique_ptr members that are forward-declared
    virtual ~Main() override;

    Main(const Main&) = delete;
    Main& operator=(const Main&) = delete;

    // Sync IBD state
    void setSyncIBD(bool zSync);
    bool isSyncIBD() const;

    // Restore/shutdown flags
    void setHasShutDown();
    bool isShuttingDown() const;
    bool isRestoring() const;
    bool isShuttongDownOrRestoring() const;

    // Shutdown
    void shutdown();
    void shutdown(bool zCompact);

    void NotifyMainListenerOfShutDown();

    // Start-up error reporting
    void setStartUpError(bool zStartError, const std::string& zMessage);
    bool isStartupError() const;
    std::string getStartupErrorMsg() const;

    // Restore flows
    void restoreReady();
    void restoreReady(bool zShutdownFinalProcs);
    void restoreReadyForSync();

    void archiveResetReady(bool zResetWallet);
    void archiveResetReady(bool zResetWallet, bool zResetCascadeTree);

    // Memory reset during sync
    void resetMemFull();

    // Restart networking
    void restartNIO();

    // Mining speed
    void setNormalAutoMineSpeed();
    void setLowPowAutoMineSpeed();
    bool isNormalMineMode() const;

    // Uptime
    long long getUptimeMilli() const;

    // Accessors (return references)
    org::minima::system::network::NetworkManager& getNetworkManager();
    org::minima::system::network::minima::NIOManager& getNIOManager();
    org::minima::system::network::webhooks::NotifyManager& getNotifyManager();
    org::minima::system::brains::TxPoWProcessor& getTxPoWProcessor();
    org::minima::system::brains::TxPoWMiner& getTxPoWMiner();
    org::minima::system::commands::sendpoll::SendPollManager& getSendPoll();

    // Keys status
    bool getAllKeysCreated() const;
    int getAllDefaultKeysSize() const;

    // Notify hooks
    void PostNotifyEvent(const std::string& zEvent, const org::minima::utils::json::JSONObject& zData);
    void PostNotifyEvent(const std::string& zEvent, const org::minima::utils::json::JSONObject& zData, const std::string& zTo);

    // Send a message ONLY to the Listener
    static void NotifyMainListenerOnly(const std::string& zMessage);

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    void shutdownGenProcs();
    void shutdownFinalProcs();
    void shutdownFinalProcs(bool zShutDownFinalProcs);
    void doGenesis();

    // Helper to extract TxPoW from Message any
    static org::minima::objects::TxPoW* anyToTxPoWPtr(const std::any& a);
    static org::minima::objects::TxPoW* ensureRaw(org::minima::objects::TxPoW& ref);

private:
    // Startup error
    bool        STARTUP_ERROR {false};
    std::string STARTUP_ERROR_MSG;

    // Timers
    long long CLEANDB_RAM_TIMER   = static_cast<long long>(1000) * 60 * 30;
    long long CLEANDB_SQL_TIMER   = static_cast<long long>(1000) * 60 * 60 * 12;
    long long SYSTEMCLEAN_TIMER   = static_cast<long long>(1000) * 60 * 5;
    long long MAIN_AUTOBACKUP_MYSQL_TIMER = static_cast<long long>(1000) * 60 * 60 * 2;
    long long AUTOBACKUP_TIMER    = static_cast<long long>(1000) * 60 * 60 * 24;
    long long NETRESET_TIMER      = static_cast<long long>(1000) * 60 * 60 * 24;
    long long INIT_KEYS_TIMER     = static_cast<long long>(1000) * 10;
    long long CHECKER_TIMER       = static_cast<long long>(1000) * 180;
    long long P2PNET_TIMER     = static_cast<long long>(1000) * 60 * 30;

    // MySQL guard
    bool MYSQL_IMPORTING_NO_ACTION {false};

    // Mining auto-timer
    long long AUTOMINE_TIMER = static_cast<long long>(1000) * 50;

    // Uptime
    long long mUptimeMilli {0};

    // Shutdown flags
    bool mShuttingdown {false};
    bool mHaveShutDownFinalProcs {false};
    bool mRestoring {false};
    bool mSyncIBD {false};

    // Mining mode
    bool mNormalMineMode {true};

    // Old tip check
    org::minima::objects::base::MiniData mOldTip;

    // Managers / processors
    std::unique_ptr<org::minima::system::brains::TxPoWProcessor>              mTxPoWProcessor;
    std::unique_ptr<org::minima::system::brains::TxPoWMiner>                  mTxPoWMiner;
    std::unique_ptr<org::minima::system::network::NetworkManager>             mNetwork;
    std::unique_ptr<org::minima::system::commands::sendpoll::SendPollManager> mSendPoll;
    std::unique_ptr<org::minima::system::network::webhooks::NotifyManager>    mNotifyManager;

    // Default keys created
    bool mInitKeysCreated {false};

    // Has listener been notified of shutdown
    bool mShutDownSentToListener {false};

    // Static singleton pointer and listener
    static Main* sMainInstance;
    static org::minima::utils::messages::MessageListener* sMinimaListener;
};

} // namespace system
} // namespace minima
} // namespace org