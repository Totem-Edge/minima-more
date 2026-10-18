#include "org/minima/system/params/param_configurer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"
#include "org/minima/system/network/p2p/params/p2_p_params.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/r_p_c_client.hpp"

// These project headers are required for the consumers (static globals/params)
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"
#include "org/minima/system/params/test_params.hpp"

namespace fs = std::filesystem;

namespace org {
namespace minima {
namespace system {
namespace params {

using org::minima::system::network::p2p::P2PFunctions;
using org::minima::system::network::p2p::params::P2PParams;
using org::minima::utils::MinimaLogger;
using org::minima::utils::RPCClient;
using org::minima::objects::base::MiniNumber;

namespace {

// trim helper similar to Java String.trim()
inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return std::string();
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Metadata entry for each ParamKeys
struct ParamKeyEntry {
    ParamConfigurer::ParamKeys id;
    const char* key;
    const char* help;
    std::function<void(const std::string&, ParamConfigurer&)> consumer;
};

// Build the full table of ParamKeys metadata and consumers.
const std::vector<ParamKeyEntry>& getParamKeyEntries() {
    static const std::vector<ParamKeyEntry> entries = {
        { ParamConfigurer::ParamKeys::data, "data", "Specify the data folder ( defaults to ~/.minima )",
          [](const std::string& args, ParamConfigurer&) {
              fs::path dataFolder(args);
              fs::path minimafolder = dataFolder / org::minima::system::params::GlobalParams::MINIMA_BASE_VERSION;
              std::error_code ec;
              fs::create_directories(minimafolder, ec);
              org::minima::system::params::GeneralParams::DATA_FOLDER = minimafolder.string();
          } },
        { ParamConfigurer::ParamKeys::dbpassword, "dbpassword", "Main Wallet / SQL AES password - MUST be specified on first launch. CANNOT be changed later.",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::IS_MAIN_DBPASSWORD_SET = true;
              org::minima::system::params::GeneralParams::MAIN_DBPASSWORD = args;
          } },
        { ParamConfigurer::ParamKeys::basefolder, "basefolder", "Specify a default file creation / backup / restore folder",
          [](const std::string& args, ParamConfigurer&) {
              fs::path backupfolder(args);
              std::error_code ec;
              fs::create_directories(backupfolder, ec);
              org::minima::system::params::GeneralParams::BASE_FILE_FOLDER = backupfolder.string();
          } },
        { ParamConfigurer::ParamKeys::host, "host", "Specify the host IP",
          [](const std::string& arg, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::MINIMA_HOST = arg;
              org::minima::system::params::GeneralParams::IS_HOST_SET = true;
          } },
        { ParamConfigurer::ParamKeys::port, "port", "Specify the Minima port",
          [](const std::string& arg, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::MINIMA_PORT = std::stoi(arg);
          } },
        { ParamConfigurer::ParamKeys::rpc, "rpc", "Specify the RPC port",
          [](const std::string&, ParamConfigurer&) {
              MinimaLogger::log(std::string("-rpc is no longer in use. Your RPC port is: ")
                                + std::to_string(org::minima::system::params::GeneralParams::RPC_PORT));
          } },
        { ParamConfigurer::ParamKeys::rpcenable, "rpcenable", "Enable rpc",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::RPC_ENABLED = (args == "true");
          } },
        { ParamConfigurer::ParamKeys::rpcpassword, "rpcpassword", "Set Basic Auth password for RPC calls ( Use with SSL / stununel )",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::RPC_PASSWORD = args;
              org::minima::system::params::GeneralParams::RPC_AUTHENTICATE = true;
          } },
        { ParamConfigurer::ParamKeys::rpcssl, "rpcssl", "Use Self Signed SSL cert to run RPC",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::RPC_SSL = (args == "true");
          } },
        { ParamConfigurer::ParamKeys::rpccrlf, "rpccrlf", "Use CRLF at the end of the RPC headers (NodeJS)",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::RPC_CRLF = (args == "true");
          } },
        { ParamConfigurer::ParamKeys::shownetcalls, "shownetcalls", "Show all the network calls",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::SHOW_NETWORK_CALLS = true;
                  org::minima::system::params::GeneralParams::SHOW_NETWORK_POLLS = true;
              } else {
                  org::minima::system::params::GeneralParams::SHOW_NETWORK_CALLS = false;
              }
          } },
        { ParamConfigurer::ParamKeys::shownetcallsnopoll, "shownetcallsnopoll", "Show all the network calls except poll messages",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::SHOW_NETWORK_CALLS = true;
                  org::minima::system::params::GeneralParams::SHOW_NETWORK_POLLS = false;
              } else {
                  org::minima::system::params::GeneralParams::SHOW_NETWORK_CALLS = false;
              }
          } },
        { ParamConfigurer::ParamKeys::allowallip, "allowallip", "Allow all IP for Networking",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::ALLOW_ALL_IP = (args == "true");
          } },
        { ParamConfigurer::ParamKeys::archive, "archive", "Run an Archive node - store all data / cascade for resync",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::ARCHIVE = (args == "true");
          } },
        { ParamConfigurer::ParamKeys::conf, "conf", "Specify a configuration file (absolute)",
          [](const std::string&, ParamConfigurer&) {
              // do nothing
          } },
        { ParamConfigurer::ParamKeys::daemon, "daemon", "Run in daemon mode with no stdin input ( services )",
          [](const std::string& args, ParamConfigurer& configurer) {
              if (args == "true") {
                  configurer.setDaemon(true);
              }
          } },
        { ParamConfigurer::ParamKeys::isclient, "isclient", "Tells the P2P System that this node can't accept incoming connections",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::IS_ACCEPTING_IN_LINKS = false;
              }
          } },
        { ParamConfigurer::ParamKeys::desktop, "desktop", "Use Desktop settings - this node can't accept incoming connections",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::IS_DESKTOP = true;
                  org::minima::system::params::GeneralParams::IS_ACCEPTING_IN_LINKS = false;
              }
          } },
        { ParamConfigurer::ParamKeys::server, "server", "Use Server settings - this node can accept incoming connections",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::IS_ACCEPTING_IN_LINKS = true;
              }
          } },
        { ParamConfigurer::ParamKeys::mobile, "mobile", "Sets this device to a mobile device - used for metrics only",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::IS_ACCEPTING_IN_LINKS = false;
                  org::minima::system::params::GeneralParams::IS_MOBILE = true;
              }
          } },
        { ParamConfigurer::ParamKeys::jnlp, "jnlp", "Are we running from the JNLP",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::IS_JNLP = true;
              }
          } },
        { ParamConfigurer::ParamKeys::showparams, "showparams", "Show startup params on launch",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::SHOW_PARAMS = true;
              }
          } },
        { ParamConfigurer::ParamKeys::nop2p, "nop2p", "Disable the automatic P2P system",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::P2P_ENABLED = false;
              }
          } },
        { ParamConfigurer::ParamKeys::noshutdownhook, "noshutdownhook", "Do not use the shutdown hook (Android)",
          [](const std::string&, ParamConfigurer& configurer) {
              configurer.setShutdownHook(false);
          } },
        { ParamConfigurer::ParamKeys::noconnect, "noconnect", "Stops the P2P system from connecting to other nodes until it's been connected to",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::NOCONNECT = true;
              }
          } },
        { ParamConfigurer::ParamKeys::p2prootnode, "p2prootnode", "Specify the initial P2P host:port to connect to",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::P2P_ROOTNODE = args;
          } },
        { ParamConfigurer::ParamKeys::p2pnodes, "p2pnodes", "Specify a list of nodes (or an URL to a file with the list) to use IF your peers list is empty",
          [](const std::string& args, ParamConfigurer&) {
              if (args.rfind("http", 0) == 0) {
                  try {
                      MinimaLogger::log(std::string("Downloading peers list from: ") + args);
                      std::string peers = RPCClient::sendGET(args);
                      org::minima::system::params::GeneralParams::P2P_ADDNODES = peers;
                      MinimaLogger::log(std::string("P2P Nodes file downloaded from : ") + args);
                  } catch (const std::exception& e) {
                      MinimaLogger::log(std::string("Error trying -p2pnodes URL:") + args + " " + e.what());
                  }
              } else {
                  org::minima::system::params::GeneralParams::P2P_ADDNODES = args;
              }
          } },
        { ParamConfigurer::ParamKeys::p2ploglevelinfo, "p2p-log-level-info", "Set the P2P log level to info",
          [](const std::string&, ParamConfigurer&) {
              P2PParams::LOG_LEVEL = P2PFunctions::Level::INFO;
          } },
        { ParamConfigurer::ParamKeys::p2plogleveldebug, "p2p-log-level-debug", "Set the P2P log level to debug",
          [](const std::string&, ParamConfigurer&) {
              P2PParams::LOG_LEVEL = P2PFunctions::Level::DEBUG;
          } },
        { ParamConfigurer::ParamKeys::p2p2, "p2p2", "Enable the new P2P2 system",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::P2P2_ENABLED = true;
                  org::minima::system::params::GeneralParams::P2P_ENABLED = false;
              }
          } },
        { ParamConfigurer::ParamKeys::connect, "connect", "Disable the p2p and manually connect to this list of host:port",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::P2P_ENABLED = false;
              org::minima::system::params::GeneralParams::CONNECT_LIST = args;
          } },
        { ParamConfigurer::ParamKeys::clean, "clean", "Wipe data folder at startup",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::CLEAN = true;
              }
          } },
        { ParamConfigurer::ParamKeys::nodefaultminidapps, "nodefaultminidapps", "Do NOT install the default MiniDAPPs",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::DEFAULT_MINIDAPPS = false;
              }
          } },
        { ParamConfigurer::ParamKeys::nosyncibd, "nosyncibd", "Do not sync IBD (for testing)",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::NO_SYNC_IBD = true;
              }
          } },
        { ParamConfigurer::ParamKeys::syncibdlogs, "syncibdlogs", "Show detailed SYNC_IBD logs",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::IBDSYNC_LOGS = true;
              }
          } },
        { ParamConfigurer::ParamKeys::megammr, "megammr", "Are we running in MEGA MMR mode",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::IS_MEGAMMR = true;
              }
          } },
        { ParamConfigurer::ParamKeys::notifyalltxpow, "notifyalltxpow", "Send notification messages for ALL TxPoW (not just relevant)",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::NOTIFY_ALL_TXPOW = true;
              }
          } },
        { ParamConfigurer::ParamKeys::slavenode, "slavenode", "Connect to this node only and only accept TxBlock messages.",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::CONNECT_LIST = args;
              org::minima::system::params::GeneralParams::P2P_ENABLED = false;
              org::minima::system::params::GeneralParams::TXBLOCK_NODE = true;
              org::minima::system::params::GeneralParams::NO_SYNC_IBD = true;
              org::minima::system::params::GeneralParams::IS_ACCEPTING_IN_LINKS = false;
          } },
        { ParamConfigurer::ParamKeys::limitbandwidth, "limitbandwidth", "Limit the amount sent for archive sync",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::ARCHIVESYNC_LIMIT_BANDWIDTH = true;
              }
          } },
        { ParamConfigurer::ParamKeys::genesis, "genesis", "Create a genesis block, -clean and -automine",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::CLEAN = true;
                  org::minima::system::params::GeneralParams::GENESIS = true;
              }
          } },
        { ParamConfigurer::ParamKeys::test, "test", "Use test params on a private network",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::TEST_PARAMS = true;
                  org::minima::system::params::TestParams::setTestParams();
              }
          } },
        { ParamConfigurer::ParamKeys::solo, "solo", "Run a solo/private network (-test -nop2p) and will run -genesis ONLY the first time",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                    // Set PRIVATE_ flag to trigger genesis logic (matching Java behavior)
                    org::minima::system::params::GeneralParams::PRIVATE_ = true;
                    org::minima::system::params::GeneralParams::TEST_PARAMS = true;
                    org::minima::system::params::TestParams::setTestParams();
                    org::minima::system::params::GeneralParams::P2P_ENABLED = false;
                }
          } },
        { ParamConfigurer::ParamKeys::testchainlength, "testchainlength", "Specify length of tree to keep in -test mode (default is 32)",
          [](const std::string& arg, ParamConfigurer&) {
              org::minima::system::params::TestParams::MINIMA_CASCADE_START_DEPTH = MiniNumber(trim(arg));
              if (org::minima::system::params::GeneralParams::TEST_PARAMS) {
                  org::minima::system::params::TestParams::setTestParams();
              }
          } },
        { ParamConfigurer::ParamKeys::mysqldb, "mysqldb", "Set the full MySQL DB details as username:password@host:port",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::MYSQL_DB_DETAILS = args;
          } },
        { ParamConfigurer::ParamKeys::mysqldbcoins, "mysqldbcoins", "Enable the MySQL coins db backup from CLI",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::MYSQL_DB_COINS = true;
              }
          } },
        { ParamConfigurer::ParamKeys::mysqldbdelay, "mysqldbdelay", "When running in Docker.. Delay in milli-seconds before attempting first MySQL Connection",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::MYSQL_DB_DELAY = std::stoi(args);
          } },
        { ParamConfigurer::ParamKeys::mysqlalltxpow, "mysqlalltxpow", "Store all TxPoW in MySQL when autobackup enabled.",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::MYSQL_STORE_ALLTXPOW = true;
              }
          } },
        { ParamConfigurer::ParamKeys::txpowdbstore, "txpowdbstore", "How many days to store TxPoW in the internal H2 Database (default 3)",
          [](const std::string& args, ParamConfigurer&) {
              long long days = std::stoll(trim(args));
              org::minima::system::params::GeneralParams::NUMBER_DAYS_SQLTXPOWDB = days;
              if (org::minima::system::params::GeneralParams::NUMBER_DAYS_SQLTXPOWDB < 3) {
                  org::minima::system::params::GeneralParams::NUMBER_DAYS_SQLTXPOWDB = 3;
                  MinimaLogger::log("Invalid txpowdbstore.. MUST be >= 3.. setting to 3");
              }
          } },
        { ParamConfigurer::ParamKeys::rescuenode, "rescuenode", "If you connect to a heavier chain use this MegaMMR node to resync",
          [](const std::string& arg, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::RESCUE_MEGAMMR_NODE = trim(arg);
          } },
        { ParamConfigurer::ParamKeys::help, "help", "Print this help",
          [](const std::string&, ParamConfigurer& configurer) {
              std::cout << "Minima Help\n";
              for (const auto& entry : getParamKeyEntries()) {
                  // approx format: %-20s%-15s
                  std::cout.setf(std::ios::left);
                  std::cout.width(21);
                  std::cout << std::string("-") + entry.key;
                  std::cout.unsetf(std::ios::left);
                  std::cout << entry.help << "\n";
              }
            //   std::exit(1);
            configurer.setShouldExit(true);
          } },
        { ParamConfigurer::ParamKeys::seed, "seed", "Use this BIP39 seed phrase when starting a new node",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::SEED_PHRASE = args;
              org::minima::system::params::GeneralParams::ANYSEED_PHRASE = false;
          } },
        { ParamConfigurer::ParamKeys::anyseed, "anyseed", "Use this seed (ANY phrase and does not have to be BIP39 words) when starting a new node",
          [](const std::string& args, ParamConfigurer&) {
              org::minima::system::params::GeneralParams::SEED_PHRASE = args;
              org::minima::system::params::GeneralParams::ANYSEED_PHRASE = true;
          } },
        { ParamConfigurer::ParamKeys::megaprune, "megaprune", "Prune unspendable addresses from the megammr",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::MEGAMMR_MEGAPRUNE = true;
              }
          } },
        { ParamConfigurer::ParamKeys::megaprunestate, "megaprunestate", "Prune all coins with a state (useful for exchanges)",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::MEGAMMR_MEGAPRUNE = true;
                  org::minima::system::params::GeneralParams::MEGAMMR_MEGAPRUNE_STATE = true;
              }
          } },
        { ParamConfigurer::ParamKeys::megaprunetokens, "megaprunetokens", "Prune all tokens - only keep Minima coins",
          [](const std::string& args, ParamConfigurer&) {
              if (args == "true") {
                  org::minima::system::params::GeneralParams::MEGAMMR_MEGAPRUNE = true;
                  org::minima::system::params::GeneralParams::MEGAMMR_MEGAPRUNE_TOKENS = true;
              }
          } }
    };
    return entries;
}

} // anonymous namespace

// -------- ParamConfigurer Implementation --------

ParamConfigurer::ParamConfigurer()
    : m_paramKeysToArg(), m_daemon(false), m_shutdownHook(true), m_shouldExit(false) {}

ParamConfigurer& ParamConfigurer::usingConfFile(const std::vector<std::string>& programArgs) {
    // Find "-conf <file>"
    auto confKey = std::string("-") + keyString(ParamKeys::conf);
    auto it = std::find(programArgs.begin(), programArgs.end(), confKey);
    if (it != programArgs.end()) {
        auto idx = std::distance(programArgs.begin(), it);
        if (static_cast<size_t>(idx + 1) < programArgs.size()) {
            const std::string& confFileArgValue = programArgs[idx + 1];
            fs::path confFile(confFileArgValue);
            try {
                std::ifstream in(confFile);
                if (!in) throw std::ios_base::failure("Unable to open");
                const size_t MAX_CONF_LINE_LENGTH = 65536;
                std::string line;
                while (std::getline(in, line)) {
                    if (line.length() > MAX_CONF_LINE_LENGTH) {
                        std::cout << "Config file line is too long, skipping." << std::endl;
                        continue; // Skip this giant line
                    }
                    // Split key=value
                    std::string key;
                    std::string value;
                    auto pos = line.find('=');
                    if (pos == std::string::npos) {
                        key = line;
                        value = "";
                    } else {
                        key = line.substr(0, pos);
                        value = line.substr(pos + 1);
                    }
                    // Map to ParamKeys
                    auto pk = toParamKey(key);
                    if (pk.has_value()) {
                        m_paramKeysToArg[*pk] = value;
                    }
                }
            } catch (...) {
                std::cout << "Unable to read conf file." << std::endl;
                std::exit(1);
            }
        }
    }
    return *this;
}

ParamConfigurer& ParamConfigurer::usingEnvVariables(const std::unordered_map<std::string, std::string>& envVariableMap) {
    // Filter minima_ prefix (case-insensitive)
    for (const auto& kv : envVariableMap) {
        std::string key = kv.first;
        std::string value = kv.second; // no null in C++
        std::string lower;
        lower.resize(key.size());
        std::transform(key.begin(), key.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        const std::string prefix = "minima_";
        if (lower.rfind(prefix, 0) == 0) {
            std::string stripped = lower.substr(prefix.size());
            auto pk = toParamKey(stripped);
            if (pk.has_value()) {
                m_paramKeysToArg[*pk] = value;
            }
        }
    }
    return *this;
}

ParamConfigurer& ParamConfigurer::usingProgramArgs(const std::vector<std::string>& programArgs) {
    size_t arglen = programArgs.size();
    size_t index = 0;
    while (index < arglen) {
        const std::string& arg = programArgs[index];
        const size_t imuCounter = index;
        auto paramKeyOpt = progArgsToParamKey(arg);
        if (paramKeyOpt.has_value()) {
            auto nextVal = lookAheadToNonParamKeyArg(programArgs, imuCounter);
            m_paramKeysToArg[*paramKeyOpt] = nextVal.has_value() ? *nextVal : "true";
        }
        ++index;
    }
    return *this;
}

ParamConfigurer& ParamConfigurer::configure() {
    // Apply each consumer
    for (const auto& kv : m_paramKeysToArg) {
        auto pk = kv.first;
        const std::string& value = kv.second;
        // Find consumer
        const auto& entries = getParamKeyEntries();
        auto it = std::find_if(entries.begin(), entries.end(),
                               [pk](const ParamKeyEntry& e){ return e.id == pk; });
        if (it != entries.end() && it->consumer) {
            it->consumer(value, *this);
        }
    }

    // Display the params if requested
    if (org::minima::system::params::GeneralParams::SHOW_PARAMS) {
        MinimaLogger::log("Config Parameters");
        for (const auto& kv : m_paramKeysToArg) {
            std::string msg = keyString(kv.first);
            msg += ":";
            msg += kv.second;
            MinimaLogger::log(msg);
        }
    }

    return *this;
}

bool ParamConfigurer::isDaemon() const {
    return m_daemon;
}

bool ParamConfigurer::isShutDownHook() const {
    return m_shutdownHook;
}

bool ParamConfigurer::checkParams(const std::string& zFullParams) {
    // Tokenize by space, skip empty tokens (similar to Java StringTokenizer with " ")
    std::vector<std::string> args;
    std::string tok;
    std::istringstream iss(zFullParams);
    while (iss >> tok) {
        if (!tok.empty()) args.emplace_back(tok);
    }
    return checkParams(args);
}

bool ParamConfigurer::checkParams(const std::vector<std::string>& zParams) {
    try {
        ParamConfigurer configurer;
        configurer.usingProgramArgs(zParams).configure();
    } catch (...) {
        return false;
    }
    return true;
}


void ParamConfigurer::setShouldExit(bool v) {
    m_shouldExit = v;
}

bool ParamConfigurer::shouldExit() const {
    return m_shouldExit;
}

std::optional<std::string> ParamConfigurer::lookAheadToNonParamKeyArg(const std::vector<std::string>& programArgs, std::size_t currentIndex) {
    if (currentIndex + 1 <= programArgs.size() - 1) {
        auto maybeKey = progArgsToParamKey(programArgs[currentIndex + 1]);
        if (!maybeKey.has_value()) {
            return programArgs[currentIndex + 1];
        }
    }
    return std::nullopt;
}

std::optional<ParamConfigurer::ParamKeys> ParamConfigurer::progArgsToParamKey(const std::string& str) {
    if (!str.empty() && str[0] == '-') {
        const std::string stripped = str.substr(1);
        auto pk = toParamKey(stripped);
        if (!pk.has_value()) {
            throw UnknownArgumentException(str);
        }
        return pk;
    }
    return std::nullopt;
}

std::optional<ParamConfigurer::ParamKeys> ParamConfigurer::toParamKey(const std::string& keystr) {
    const auto& entries = getParamKeyEntries();
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&keystr](const ParamKeyEntry& e){ return keystr == e.key; });
    if (it != entries.end()) return it->id;
    return std::nullopt;
}

std::string ParamConfigurer::keyString(ParamConfigurer::ParamKeys key) {
    const auto& entries = getParamKeyEntries();
    auto it = std::find_if(entries.begin(), entries.end(),
                           [key](const ParamKeyEntry& e){ return e.id == key; });
    if (it != entries.end()) {
        return std::string(it->key);
    }
    return std::string();
}

std::string ParamConfigurer::helpString(ParamConfigurer::ParamKeys key) {
    const auto& entries = getParamKeyEntries();
    auto it = std::find_if(entries.begin(), entries.end(),
                           [key](const ParamKeyEntry& e){ return e.id == key; });
    if (it != entries.end()) {
        return std::string(it->help);
    }
    return std::string();
}

const std::vector<ParamConfigurer::ParamKeys>& ParamConfigurer::allKeys() {
    static std::vector<ParamConfigurer::ParamKeys> keys = []{
        std::vector<ParamConfigurer::ParamKeys> v;
        for (const auto& e : getParamKeyEntries()) v.push_back(e.id);
        return v;
    }();
    return keys;
}

ParamConfigurer::UnknownArgumentException::UnknownArgumentException(const std::string& arg)
    : std::runtime_error(std::string("Unknown argument : ") + arg) {}

void ParamConfigurer::setDaemon(bool v) {
    m_daemon = v;
}

void ParamConfigurer::setShutdownHook(bool v) {
    m_shutdownHook = v;
}

} // namespace params
} // namespace system
} // namespace minima
} // namespace org