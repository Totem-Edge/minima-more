#include "org/minima/system/params/general_params.hpp"

namespace org {
namespace minima {
namespace system {
namespace params {

// Static member definitions with Java initial values

bool GeneralParams::IS_MOBILE = false;
bool GeneralParams::IS_DESKTOP = false;
bool GeneralParams::IS_JNLP = false;
bool GeneralParams::IS_ACCEPTING_IN_LINKS = true;

bool GeneralParams::PRIVATE_ = false;
bool GeneralParams::GENESIS = false;
bool GeneralParams::CLEAN = false;
bool GeneralParams::ARCHIVE = false;
bool GeneralParams::TXBLOCK_NODE = false;

bool GeneralParams::IS_MAIN_DBPASSWORD_SET = false;
std::string GeneralParams::MAIN_DBPASSWORD = "minima";

std::string GeneralParams::DATA_FOLDER = "";
std::string GeneralParams::BASE_FILE_FOLDER = "";

std::string GeneralParams::MINIMA_HOST = "";
bool GeneralParams::IS_HOST_SET = false;

int GeneralParams::MINIMA_PORT = 9001;
int GeneralParams::RPC_PORT = GeneralParams::MINIMA_PORT + 4;

bool GeneralParams::TEST_PARAMS = false;
bool GeneralParams::P2P_ENABLED = true;
std::string GeneralParams::P2P_ROOTNODE = "";
std::string GeneralParams::P2P_ADDNODES = "";

bool GeneralParams::P2P2_ENABLED = false;
bool GeneralParams::ALLOW_ALL_IP = false;
bool GeneralParams::NOCONNECT = false;
std::string GeneralParams::CONNECT_LIST = "";

bool GeneralParams::SHOW_PARAMS = false;
bool GeneralParams::NO_SYNC_IBD = false;

int GeneralParams::MAX_RELAY_OUTPUTCOINS = 32;
std::int64_t GeneralParams::MAX_RELAY_STORESTATESIZE = 65536;

org::minima::objects::base::MiniNumber GeneralParams::MAX_SPLIT_COINS = org::minima::objects::base::MiniNumber(20);

std::int64_t GeneralParams::NUMBER_DAYS_SQLTXPOWDB = 3;
std::int64_t GeneralParams::NUMBER_HOURS_RAMTXPOWDB = 1;
std::int64_t GeneralParams::NUMBER_DAYS_ARCHIVE = 50;

std::int64_t GeneralParams::USER_PULSE_FREQ = 1000LL * 60LL * 10LL;

bool GeneralParams::DEBUGFLAG = false;
std::string GeneralParams::DEBUGVAR = "";

bool GeneralParams::SCRIPTLOGS = false;
bool GeneralParams::MINING_LOGS = false;
bool GeneralParams::NETWORKING_LOGS = false;
bool GeneralParams::IBDSYNC_LOGS = false;
bool GeneralParams::BLOCK_LOGS = false;

bool GeneralParams::ARCHIVESYNC_LIMIT_BANDWIDTH = false;

bool GeneralParams::RPC_CRLF = false;

bool GeneralParams::DEFAULT_MINIDAPPS = true;

bool GeneralParams::RPC_ENABLED = false;
bool GeneralParams::RPC_AUTHENTICATE = false;
bool GeneralParams::RPC_SSL = false;
std::string GeneralParams::RPC_AUTHSTYLE = "basic";
std::string GeneralParams::RPC_PASSWORD = "none";

std::string GeneralParams::SEED_PHRASE = "";
bool GeneralParams::ANYSEED_PHRASE = false;

bool GeneralParams::PEERSCHECKER_lOG = false;

bool GeneralParams::IS_MEGAMMR = false;

bool GeneralParams::DB_IGNORE_LOCKS = false;

bool GeneralParams::NOTIFY_ALL_TXPOW = false;

std::string GeneralParams::RESCUE_MEGAMMR_NODE = "";

std::string GeneralParams::MYSQL_DB_DETAILS = "";
int GeneralParams::MYSQL_DB_DELAY = 0;
bool GeneralParams::MYSQL_DB_COINS = false;

bool GeneralParams::MYSQL_STORE_ALLTXPOW = false;

bool GeneralParams::MEGAMMR_MEGAPRUNE = false;
bool GeneralParams::MEGAMMR_MEGAPRUNE_STATE = false;
bool GeneralParams::MEGAMMR_MEGAPRUNE_TOKENS = false;

bool GeneralParams::SHOW_NETWORK_CALLS = false;
bool GeneralParams::SHOW_NETWORK_POLLS = true;

void GeneralParams::resetDefaults() {
    IS_MOBILE = false;
    IS_DESKTOP = false;
    IS_JNLP = false;
    IS_ACCEPTING_IN_LINKS = true;
    GENESIS = false;
    PRIVATE_ = false;
    CLEAN = false;
    ARCHIVE = false;
    TXBLOCK_NODE = false;
    IS_MAIN_DBPASSWORD_SET = false;
    MAIN_DBPASSWORD = "minima";
    DATA_FOLDER = "";
    BASE_FILE_FOLDER = "";
    MINIMA_HOST = "";
    IS_HOST_SET = false;
    MINIMA_PORT = 9001;
    RPC_PORT = MINIMA_PORT + 4;
    TEST_PARAMS = false;
    P2P_ENABLED = true;
    P2P_ROOTNODE = "";
    P2P_ADDNODES = "";
    ALLOW_ALL_IP = false;
    NOCONNECT = false;
    CONNECT_LIST = "";
    SHOW_PARAMS = false;
    NO_SYNC_IBD = false;
    MAX_RELAY_OUTPUTCOINS = 15;
    MAX_SPLIT_COINS = org::minima::objects::base::MiniNumber(10);
    NUMBER_DAYS_SQLTXPOWDB = 3;
    NUMBER_HOURS_RAMTXPOWDB = 1;
    NUMBER_DAYS_ARCHIVE = 50;
    USER_PULSE_FREQ = 1000LL * 60LL * 10LL;
    DEBUGFLAG = false;
    DEBUGVAR = "";
    SCRIPTLOGS = false;
    MINING_LOGS = false;
    NETWORKING_LOGS = false;
    IBDSYNC_LOGS = false;
    BLOCK_LOGS = false;
    ARCHIVESYNC_LIMIT_BANDWIDTH = false;
    RPC_CRLF = false;
    DEFAULT_MINIDAPPS = true;
    RPC_ENABLED = false;
    RPC_AUTHENTICATE = false;
    RPC_SSL = false;
    RPC_AUTHSTYLE = "basic";
    RPC_PASSWORD = "none";
    SEED_PHRASE = "";
    ANYSEED_PHRASE = false;
    PEERSCHECKER_lOG = false;
    IS_MEGAMMR = false;
    DB_IGNORE_LOCKS = false;
    NOTIFY_ALL_TXPOW = false;
    RESCUE_MEGAMMR_NODE = "";

    MYSQL_STORE_ALLTXPOW = false;
    MYSQL_DB_DETAILS = "";
    MYSQL_DB_DELAY = 0;
    MYSQL_DB_COINS = false;

    MEGAMMR_MEGAPRUNE = false;
    MEGAMMR_MEGAPRUNE_STATE = false;
    MEGAMMR_MEGAPRUNE_TOKENS = false;

    P2P2_ENABLED = false;
    SHOW_NETWORK_CALLS = false;
    // Note: SHOW_NETWORK_POLLS retains its initial value (true), as in Java.
    // Note: MAX_RELAY_STORESTATESIZE retains its initial value (65536), as in Java.
}

} // namespace params
} // namespace system
} // namespace minima
} // namespace org