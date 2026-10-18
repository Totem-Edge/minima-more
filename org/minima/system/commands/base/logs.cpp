#include "org/minima/system/commands/base/logs.hpp"

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

logs::logs()
    : org::minima::system::commands::Command(
          "logs",
          "(scripts:) (mining:) (maxima:) (networking:) (blocks:) (ibd:) (txpowdb:) - Enable full logs for various parts of Minima") {
}

std::string logs::getFullHelp() const {
    return std::string("\nlogs\n")
         + "\n"
         + "Enable detailed logs for script errors or mining activity.\n"
         + "\n"
         + "scripts: (optional)\n"
         + "    true or false, true turns on detailed logs for script errors.\n"
         + "\n"
         + "mining: (optional)\n"
         + "    true or false, true turns on detailed logs for mining start/end activity.\n"
         + "\n"
         + "networking: (optional)\n"
         + "    true or false, true turns on detailed logs for Network Messages.\n"
         + "\n"
         + "blocks: (optional)\n"
         + "    true or false, true turns on detailed logs for Blocks.\n"
         + "\n"
         + "ibd: (optional)\n"
         + "    true or false, true turns on detailed logs for IBD processing.\n"
         + "\n"
         + "peerschecker: (optional)\n"
         + "    true or false, true turns on detailed logs for IBD processing.\n"
         + "\n"
         + "txpowdb: (optional)\n"
         + "    true or false, true turns on logs for adding TxPoW to TxPoWDB.\n"
         + "\n"
         + "Examples:\n"
         + "\n"
         + "logs scripts:true\n"
         + "\n"
         + "logs scripts:false mining:true\n";
}

std::vector<std::string> logs::getValidParams() const {
    return {
        "scripts",
        "mining",
        "maxima",
        "blocks",
        "networking",
        "ibd",
        "peerschecker",
        "txpowdb"
    };
}

std::unique_ptr<org::minima::utils::json::JSONObject> logs::runCommand() {
    using org::minima::system::params::GeneralParams;
    using org::minima::database::txpowdb::sql::TxPoWSqlDB;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    // scripts
    if (existsParam("scripts")) {
        std::string scripts = getParam("scripts", "false");
        if (scripts == "true") {
            GeneralParams::SCRIPTLOGS = true;
        } else {
            GeneralParams::SCRIPTLOGS = false;
        }
    }

    // mining
    if (existsParam("mining")) {
        std::string mining = getParam("mining", "false");
        if (mining == "true") {
            GeneralParams::MINING_LOGS = true;
        } else {
            GeneralParams::MINING_LOGS = false;
        }
    }

    // blocks
    if (existsParam("blocks")) {
        std::string blocks = getParam("blocks", "false");
        if (blocks == "true") {
            GeneralParams::BLOCK_LOGS = true;
        } else {
            GeneralParams::BLOCK_LOGS = false;
        }
    }

    // networking
    if (existsParam("networking")) {
        std::string networking = getParam("networking", "false");
        if (networking == "true") {
            GeneralParams::NETWORKING_LOGS = true;
        } else {
            GeneralParams::NETWORKING_LOGS = false;
        }
    }

    // ibd
    if (existsParam("ibd")) {
        std::string ibd = getParam("ibd", "false");
        if (ibd == "true") {
            GeneralParams::IBDSYNC_LOGS = true;
        } else {
            GeneralParams::IBDSYNC_LOGS = false;
        }
    }

    // peerschecker
    if (existsParam("peerschecker")) {
        std::string peerschecker = getParam("peerschecker", "false");
        if (peerschecker == "true") {
            GeneralParams::PEERSCHECKER_lOG = true;
        } else {
            GeneralParams::PEERSCHECKER_lOG = false;
        }
    }

    // txpowdb
    if (existsParam("txpowdb")) {
        std::string txpowdb = getParam("txpowdb", "false");
        if (txpowdb == "true") {
            TxPoWSqlDB::LOG_ADD_BLOCKS = true;
            TxPoWSqlDB::LOG_ADD_TRANSACTION = true;
        } else {
            TxPoWSqlDB::LOG_ADD_BLOCKS = false;
            TxPoWSqlDB::LOG_ADD_TRANSACTION = false;
        }
    }

    JSONObject resp;
    resp.put("scripts", GeneralParams::SCRIPTLOGS);
    resp.put("mining", GeneralParams::MINING_LOGS);
    resp.put("blocks", GeneralParams::BLOCK_LOGS);
    resp.put("txpowdb", TxPoWSqlDB::LOG_ADD_BLOCKS);
    resp.put("networking", GeneralParams::NETWORKING_LOGS);
    resp.put("ibd", GeneralParams::IBDSYNC_LOGS);
    resp.put("peerschecker", GeneralParams::PEERSCHECKER_lOG);

    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* logs::getFunction() {
    return new logs();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org