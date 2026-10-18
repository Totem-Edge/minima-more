#pragma once

#include "org/minima/objects/base/mini_number.hpp"

#include <string>
#include <cstdint>

namespace org {
namespace minima {
namespace system {
namespace params {

class GeneralParams {
public:
    static bool IS_MOBILE;
    static bool IS_DESKTOP;
    static bool IS_JNLP;
    static bool IS_ACCEPTING_IN_LINKS;

    // Renamed from Java 'PRIVATE' to avoid C++ keyword conflict
    static bool PRIVATE_;
    static bool GENESIS;
    static bool CLEAN;
    static bool ARCHIVE;
    static bool TXBLOCK_NODE;

    static bool IS_MAIN_DBPASSWORD_SET;
    static std::string MAIN_DBPASSWORD;

    static std::string DATA_FOLDER;
    static std::string BASE_FILE_FOLDER;

    static std::string MINIMA_HOST;
    static bool IS_HOST_SET;

    static int MINIMA_PORT;
    static int RPC_PORT;

    static bool TEST_PARAMS;
    static bool P2P_ENABLED;
    static std::string P2P_ROOTNODE;
    static std::string P2P_ADDNODES;

    static bool P2P2_ENABLED;
    static bool ALLOW_ALL_IP;
    static bool NOCONNECT;
    static std::string CONNECT_LIST;

    static bool SHOW_PARAMS;
    static bool NO_SYNC_IBD;

    static int MAX_RELAY_OUTPUTCOINS;
    static std::int64_t MAX_RELAY_STORESTATESIZE;

    static org::minima::objects::base::MiniNumber MAX_SPLIT_COINS;

    static std::int64_t NUMBER_DAYS_SQLTXPOWDB;
    static std::int64_t NUMBER_HOURS_RAMTXPOWDB;
    static std::int64_t NUMBER_DAYS_ARCHIVE;

    static std::int64_t USER_PULSE_FREQ;

    static bool DEBUGFLAG;
    static std::string DEBUGVAR;

    static bool SCRIPTLOGS;
    static bool MINING_LOGS;
    static bool NETWORKING_LOGS;
    static bool IBDSYNC_LOGS;
    static bool BLOCK_LOGS;

    static bool ARCHIVESYNC_LIMIT_BANDWIDTH;

    static bool RPC_CRLF;

    static bool DEFAULT_MINIDAPPS;

    static bool RPC_ENABLED;
    static bool RPC_AUTHENTICATE;
    static bool RPC_SSL;
    static std::string RPC_AUTHSTYLE;
    static std::string RPC_PASSWORD;

    static std::string SEED_PHRASE;
    static bool ANYSEED_PHRASE;

    static bool PEERSCHECKER_lOG;

    static bool IS_MEGAMMR;

    // Debug flag to disable all MinimaDB RW locking (matches Java GeneralParams.DB_IGNORE_LOCKS)
    static bool DB_IGNORE_LOCKS;

    static bool NOTIFY_ALL_TXPOW;

    static std::string RESCUE_MEGAMMR_NODE;

    static std::string MYSQL_DB_DETAILS;
    static int MYSQL_DB_DELAY;
    static bool MYSQL_DB_COINS;

    static bool MYSQL_STORE_ALLTXPOW;

    static bool MEGAMMR_MEGAPRUNE;
    static bool MEGAMMR_MEGAPRUNE_STATE;
    static bool MEGAMMR_MEGAPRUNE_TOKENS;

    static bool SHOW_NETWORK_CALLS;
    static bool SHOW_NETWORK_POLLS;

    static void resetDefaults();
};

} // namespace params
} // namespace system
} // namespace minima
} // namespace org