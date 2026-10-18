#include "org/minima/system/commands/backup/mysqlcoins.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp" 
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/mysql/my_s_q_l_connect.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

using org::minima::database::MinimaDB;
using org::minima::database::archive::ArchiveManager;
using org::minima::database::userprefs::UserDB;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::TxBlock;
using org::minima::objects::base::MiniNumber;
using org::minima::system::commands::CommandException;
using org::minima::system::params::GeneralParams;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;
using org::minima::utils::mysql::MySQLConnect;

namespace {
// Format milliseconds since epoch as "YYYY-MM-DD HH:mm:ss.SSS" in UTC
static std::string formatDateMillis(long long millis) {
    long long secs = millis / 1000;
    long long ms = millis % 1000;

    std::time_t tt = static_cast<std::time_t>(secs);
    std::tm tmout{};
#ifdef _WIN32
    gmtime_s(&tmout, &tt);
#else
    gmtime_r(&tt, &tmout);
#endif

    char base[32];
    if (std::strftime(base, sizeof(base), "%Y-%m-%d %H:%M:%S", &tmout) == 0) {
        return std::to_string(millis);
    }

    std::ostringstream oss;
    oss << base << '.' << std::setw(3) << std::setfill('0') << ms;
    return oss.str();
}

static long long nowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

} // unnamed namespace

mysqlcoins::mysqlcoins()
    : org::minima::system::commands::Command(
          "mysqlcoins",
          "Create and search a coins database from your MySQL Archive") {}

std::string mysqlcoins::getFullHelp() const {
    return std::string("\nmysqlcoins\n"
        "\n"
        "Create a coins db from your mysql data and search it.\n"
        "\n"
        "Use the same database you already use for your TxBlocks. Just creates a new table.\n"
        "\n"
        "You can use setlogin in mysql function to auto set the login details\n"
        "\n"
        "host:\n"
        "    The ip:port (or name of Docker container) running the MySQL db.\n"
        "\n"
        "database:\n"
        "    name of the MySQL db being used to store the archive db data.\n"
        "\n"
        "user:\n"
        "    MySQL user to login to as.\n"
        "\n"
        "password:\n"
        "    MySQL password for the user provided.\n"
        "\n"
        "readonly:\n"
        "    true or false, Connect in readonly mode.\n"
        "logs:\n"
        "    Show detailed logs - default false.\n"
        "\n"
        "where:\n"
        "    The search criteria. String data MUST be in single quotes. You can use multiple parameters.\n"
        "\n"
        "maxcoins:\n"
        "    The maximum numnber of coins to add. The update can take a VERY long time so this way you can limit it.\n"
        "\n"
        "action:\n"
        "    info : Get information about the Coins DB.\n"
        "    wipe : Wipe the Coins DB.\n"
        "    update : Update the coins db from the latest coin added with MySQL data.\n"
        "    search : Perform a search on the data. You can specify any valid query param or do a specific address check.\n"
        "           Other params :\n"
        "           query:the full sql query\n"
        "\n"
        "           address:the address to check for\n"
        "           spent:true/false\n"
        "           limit:limit the number of rows returned\n"
        "\n"
        "Examples:\n"
        "\n"
        "mysqlcoins host:127.0.0.1:3306 database:coinsdb user:myuser password:myuser action:update maxcoins:100\n"
        "\n"
        "mysqlcoins (If you have not setlogin in mysql function)..LOGIN_DETAILS.. action:search where:\"address='0x791E78C60652B0E19B8FE9EB035B122B261490C477FD76E38C0C928187076103'\"\n"
        "\n"
        "mysqlcoins action:search where:\"address='0x791E78C60652B0E19B8FE9EB035B122B261490C477FD76E38C0C928187076103' spent:false limit:1\"\n"
        "\n"
        "mysqlcoins action:search address:Mx87DE..\n"
        "\n"
        "mysqlcoins action:search query:\"address='0x791E78C60652B0E19B8FE9EB035B122B261490C477FD76E38C0C928187076103' AND state LIKE '%0xFFEEDD%' LIMIT 10\"\n");
}

std::vector<std::string> mysqlcoins::getValidParams() const {
    return std::vector<std::string>{
        "action","host","database","user","password","logs","readonly","query",
        "where","maxblocks","maxcoins","hidetoken","address","spent","limit","enable"
    };
}

std::unique_ptr<JSONObject> mysqlcoins::runCommand() {
    auto ret = getJSONReply();

    if (GeneralParams::IS_MOBILE) {
        throw CommandException("Sorry - MySQL does not work on Android..");
    }

    std::string action = getParam("action", "info");

    // Get the details
    UserDB& udb = MinimaDB::getDB()->getUserDB();

    std::string host;
    std::string dbname;
    std::string user;
    std::string password;

    bool autologindetail = MinimaDB::getDB()->getUserDB().getAutoLoginDetailsMySQL();
    if (autologindetail) {
        host     = udb.getAutoMySQLHost();
        dbname   = udb.getAutoMySQLDB();
        user     = udb.getAutoMySQLUser();
        password = udb.getAutoMySQLPassword();
    } else {
        host     = getParam("host");
        dbname   = getParam("database");
        user     = getParam("user");
        password = getParam("password");
    }

    bool logs = getBooleanParam("logs", false);
    int maxblocks = getNumberParam("maxblocks", MiniNumber::BILLION())->getAsInt();
    int maxcoins  = getNumberParam("maxcoins",  MiniNumber::BILLION())->getAsInt();

    bool readonly = getBooleanParam("readonly", false);
    MySQLConnect mysql(host, dbname, user, password, readonly);
    mysql.init();

    // Get the ArchiveManager (unused here; ensures DB is loaded as in Java)
    ArchiveManager& arch = MinimaDB::getDB()->getArchive();
    (void)arch;

    if (action == "info") {
        long long total = mysql.getTotalCoins();
        long long lastblock = mysql.getMaxCoinBlock();

        JSONObject resp;
        resp.put("lastblock", lastblock);
        resp.put("total", total);

        bool autobackup = MinimaDB::getDB()->getUserDB().getAutoBackupMySQLCoins();
        resp.put("autobackup", autobackup);

        ret->put("response", resp);

    } else if (action == "wipe") {
        MinimaLogger::log("Wiping CoinsDB..");

        mysql.wipeCoinsDB();

        // Keep original misspelling for parity
        ret->put("respnse", std::string("CoinsDB wiped"));

    } else if (action == "autobackup") {
        bool enable = getBooleanParam("enable");
        udb.setAutoBackupMySQLCoins(enable);

        if (enable) {
            udb.setAutoMySQLHost(host);
            udb.setAutoMySQLDB(dbname);
            udb.setAutoMySQLUser(user);
            udb.setAutoMySQLPassword(password);
        } else {
            udb.setAutoMySQLHost("");
            udb.setAutoMySQLDB("");
            udb.setAutoMySQLUser("");
            udb.setAutoMySQLPassword("");
        }

        JSONObject resp;
        bool autobackup = MinimaDB::getDB()->getUserDB().getAutoBackupMySQLCoins();
        resp.put("autobackup", autobackup);

        ret->put("response", resp);

    } else if (action == "update") {
        MinimaLogger::log("Updating CoinsDB..");

        long long mysqlstartblock = mysql.loadLastBlock();

        long long lastcoin = mysql.getMaxCoinBlock();
        if (lastcoin != -1) {
            MinimaLogger::log("Last coin found @ " + std::to_string(lastcoin));
            mysqlstartblock = lastcoin + 1;
        }
        MinimaLogger::log("Starting from : " + std::to_string(mysqlstartblock));

        long long firstblock = -1;
        long long endblock   = -1;

        long long startload      = mysqlstartblock;
        int coincounter          = 0;
        int blockcounter         = 0;
        long long starttimer     = nowMillis();
        long long timer          = starttimer;
        bool maxblocksreached    = false;

        while (!maxblocksreached) {
            std::vector<std::unique_ptr<TxBlock>> blocks = mysql.loadBlockRange(MiniNumber(startload));
            if (blocks.empty()) {
                break;
            }

            if (logs) {
                long long timenow = nowMillis();
                if (timenow - timer > 5000) {
                    timer = timenow;
                    MinimaLogger::log("Loading from MySQL @ " + std::to_string(startload));
                }
            }

            for (const auto& blkptr : blocks) {
                const TxBlock& block = *blkptr;
                blockcounter++;

                if (blockcounter > maxblocks) {
                    blockcounter--;
                    maxblocksreached = true;
                    break;
                }

                MiniNumber blocknum = block.getTxPoW().getBlockNumber();
                if (firstblock == -1) {
                    firstblock = blocknum.getAsLong();
                }
                long long timemilli = block.getTxPoW().getTimeMilli().getAsLong();
                std::string date = formatDateMillis(timemilli);

                // Inputs: copy to allow mutation (const accessors)
                const std::vector<CoinProof>& inputs = block.getInputCoinProofs();
                for (const CoinProof& cc : inputs) {
                    std::shared_ptr<const Coin> ccp = cc.getCoinPtr();
                    if (ccp) {
                        std::unique_ptr<Coin> coinCopy = ccp->deepCopy();
                        coinCopy->setSpent(true);
                        if (logs) {
                            MinimaLogger::log("Found Input coin @ " + blocknum.toString() + " : " + coinCopy->toJSON().toString());
                        }
                        mysql.insertCoin(*coinCopy, blocknum.getAsLong(), date);
                        ++coincounter;
                    }
                }

                // Outputs: copy to allow mutation
                const std::vector<Coin>& outputs = block.getOutputCoins();
                for (const Coin& cc : outputs) {
                    std::unique_ptr<Coin> coinCopy = cc.deepCopy();
                    coinCopy->setSpent(false);
                    coinCopy->setBlockCreated(blocknum);
                    if (logs) {
                        MinimaLogger::log("Found Output coin @ " + blocknum.toString() + " : " + coinCopy->toJSON().toString());
                    }
                    mysql.insertCoin(*coinCopy, 0, date);
                    ++coincounter;
                }

                endblock = block.getTxPoW().getBlockNumber().getAsLong();

                if (coincounter > maxcoins) {
                    MinimaLogger::log("Max coins added : " + std::to_string(coincounter) + "/" + std::to_string(maxcoins));
                    maxblocksreached = true;
                    break;
                }
            }

            startload = endblock + 1;
        }

        long long endtimer = nowMillis();
        long long timediff = endtimer - starttimer;

        JSONObject resp;
        resp.put("duration", std::to_string(timediff / 1000) + " seconds");
        resp.put("blocks", blockcounter);
        resp.put("firstblock", firstblock);
        resp.put("lastblock", endblock);
        resp.put("coinsadded", coincounter);
        resp.put("message", std::string("MySQL CoinsDB Updated.."));
        ret->put("response", resp);

    } else if (action == "search") {
        std::string sql;

        if (existsParam("where")) {
            std::string where = getParam("where");
            sql = "SELECT * FROM coins WHERE " + where;

        } else if (existsParam("address")) {
            std::string address = getAddressParam("address");

            sql = "SELECT * FROM coins "
                  "WHERE ( address='" + address + "' "
                  "OR state LIKE '%" + address + "%' )";

            if (existsParam("spent")) {
                bool spent = getBooleanParam("spent");
                if (spent) {
                    sql += " AND spent=1";
                } else {
                    sql += " AND spent=0";
                }
            }

            sql += " ORDER BY blockcreated ASC";

            if (existsParam("limit")) {
                int limit = getNumberParam("limit")->getAsInt();
                sql += " LIMIT " + std::to_string(limit);
            }

        } else if (existsParam("query")) {
            sql = getParam("query");

        } else {
            mysql.shutdown();
            throw CommandException("MUST provide where or query parameter..");
        }

        bool hidetoken = getBooleanParam("hidetoken", false);

        JSONObject res = mysql.searchCoins(sql, hidetoken);

        ret->put("response", res);

    } else {
        mysql.shutdown();
        throw CommandException("Invalid action : " + action);
    }

    mysql.shutdown();
    return ret;
}

org::minima::system::commands::Command* mysqlcoins::getFunction() {
    return new mysqlcoins();
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org