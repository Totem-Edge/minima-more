#include "org/minima/system/commands/search/history.hpp"

#include <unordered_map>
#include <unordered_set>
#include <chrono>

#include "org/minima/system/commands/command_exception.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_data.hpp"

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

#ifdef _WIN32
// No Windows-specific behavior needed currently
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;
using org::minima::objects::TxPoW;
using org::minima::objects::Transaction;
using org::minima::objects::Coin;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::base::MiniData;

history::history()
    : org::minima::system::commands::Command(
          "history",
          "(action:) (max:) (offset:) (relevant:) - Search for all relevant TxPoW") {}

std::string history::getFullHelp() const {
    return std::string("\nhistory\n"
        "\n"
        "Return all TxPoW relevant to you. Default to 100 max (can be slow)\n"
        "\n"
        "action: (optional) default is 'list'\n"
        "\t     list : List your transactions\n"
        "      size : Count how many transaction you have in History\n"
        "      customsize : use with 'where' to search TxPoWDB\n"
        "      transactions : How Many transactions in chain for 'depth' blocks"
        "\n"
        "max: (optional)\n"
        "    Maximum number of TxPoW to retrieve.\n"
        "\n"
        "offset: (optional)\n"
        "    Start the list from this point.\n"
        "\n"
        "depth: (optional)\n"
        "    How far down chain to search for transactions.\n"
        "\n"
        "relevant: (optional)\n"
        "    Do you want YOUR transactions or ALL transactions (defaults to true).\n"
        "\n"
        "where: (optional)\n"
        "    Use with customsize to search for a specific set. This is the WHERE clause in SQL query.\n"
        "\n"
        "Examples:\n"
        "\n"
        "history\n"
        "\n"
        "history max:20\n"
        "\n"
        "history action:size relevant:false\n"
        "\n"
        "history action:transactions depth:1720\n"
        "\n"
        "history action:customsize where:\"isblock=1 AND timemilli>1728037509020\"\n"
        "\n"
        "history max:20 offset:45\n");
}

std::vector<std::string> history::getValidParams() const {
    return { "depth","max","offset","action","relevant","startmilli","where" };
}

std::unique_ptr<JSONObject> history::runCommand() {
    std::unique_ptr<JSONObject> ret = getJSONReply();

    const std::string action = getParam("action", "list");
    JSONObject resp;

    const bool relevant = getBooleanParam("relevant", true);

    // Use the node's real SQL TxPoW DB (a local default-constructed TxPoWSqlDB
    // opens an empty/temporary database and returns wrong/empty results).
    auto* sqldb = org::minima::database::MinimaDB::getDB()->getTxPoWDB().getSQLDB();
    if (!sqldb) {
        throw org::minima::system::commands::CommandException("TxPoW SQL DB not available");
    }

    if (action == "size") {
        if (relevant) {
            int size = sqldb->getRelevantSize();
            resp.put("size", size);
        } else {
            using namespace std::chrono;
            const auto now = time_point_cast<milliseconds>(system_clock::now());
            const long long current_ms = now.time_since_epoch().count();
            const long long oneday = 1000LL * 60 * 60 * 24;
            const long long defaultstart = current_ms - oneday;

            std::unique_ptr<MiniNumber> startMilliParam =
                getNumberParam("startmilli", MiniNumber(defaultstart));
            long long starttime = startMilliParam->getAsLong();

            int size = sqldb->getLatestTxPoWSize(starttime);
            resp.put("startmilli", starttime);
            resp.put("size", size);
        }
    } else if (action == "customsize") {
        std::string where = getParam("where");
        int size = sqldb->customSizeQuery(where);
        resp.put("size", size);
    } else if (action == "transactions") {
        // Without TxPowTree interface in headers, cannot implement safely.
        throw org::minima::system::commands::CommandException(
            "Invalid action:transactions (TxPoWTree interface unavailable in this build)");
    } else if (action == "list") {
        int max = getNumberParam("max", org::minima::database::txpowdb::sql::TxPoWSqlDB::MAX_RELEVANT_TXPOW)->getAsInt();
        int offset = getNumberParam("offset", MiniNumber::ZERO())->getAsInt();

        // Fetch TxPoW rows
        std::vector<std::unique_ptr<TxPoW>> txps;
        if (relevant) {
            txps = sqldb->getAllRelevant(max, offset);
            resp.put("relevant", true);
        } else {
            txps = sqldb->getLatestTxPoW(max, offset);
            resp.put("relevant", false);
        }

        JSONArray txns;
        JSONArray txndetails;

        for (const auto& txp : txps) {
            // TxPoW JSON
            JSONObject txpj = txp->toJSON();
            txns.add(txpj);

            // Details JSON
            JSONObject det = getTxnDetails(*txp);
            txndetails.add(det);
        }

        resp.put("txpows", txns);
        resp.put("details", txndetails);
        resp.put("size", static_cast<int>(txns.size()));
    } else {
        throw org::minima::system::commands::CommandException("Invalid action:" + action);
    }

    ret->put("response", resp);
    return ret;
}

JSONObject history::getTxnDetails(const TxPoW& zTxPoW) {
    JSONObject ret;

    // Amount tables per token ID
    std::unordered_map<std::string, MiniNumber> inamounts;
    std::unordered_map<std::string, MiniNumber> outamounts;

    // Transaction
    const Transaction& trans = zTxPoW.getTransaction();

    // Wallet
    org::minima::database::wallet::Wallet& wal = org::minima::database::MinimaDB::getDB()->getWallet();

    // Inputs
    const auto& inputs = trans.getAllInputs();
    for (const auto& upc : inputs) {
        const Coin& cc = *upc;

        const std::string addr = cc.getAddress().to0xString();
        const std::string tok = cc.getTokenID().to0xString();

        if (wal.isAddressRelevant(addr)) {
            MiniNumber tot = MiniNumber::ZERO();
            auto it = inamounts.find(tok);
            if (it != inamounts.end()) {
                tot = it->second;
            }

            if (tok == "0x00") {
                tot = tot.add(cc.getAmount());
            } else {
                tot = tot.add(cc.getTokenAmount());
            }

            inamounts[tok] = tot;
        }
    }

    // Outputs
    const auto& outputs = trans.getAllOutputs();
    for (const auto& upc : outputs) {
        const Coin& cc = *upc;

        const std::string addr = cc.getAddress().to0xString();
        const std::string tok = cc.getTokenID().to0xString();

        if (wal.isAddressRelevant(addr)) {
            MiniNumber tot = MiniNumber::ZERO();
            auto it = outamounts.find(tok);
            if (it != outamounts.end()) {
                tot = it->second;
            }

            if (tok == "0x00") {
                tot = tot.add(cc.getAmount());
            } else {
                tot = tot.add(cc.getTokenAmount());
            }

            outamounts[tok] = tot;
        }
    }

    // Build JSON objects and collect all token IDs
    std::unordered_set<std::string> alltoks;

    JSONObject ins;
    for (const auto& kv : inamounts) {
        ins.put(kv.first, kv.second.toString());
        alltoks.insert(kv.first);
    }

    JSONObject outs;
    for (const auto& kv : outamounts) {
        outs.put(kv.first, kv.second.toString());
        alltoks.insert(kv.first);
    }

    // Differences: out - in
    JSONObject diffs;
    for (const auto& key : alltoks) {
        MiniNumber in = MiniNumber::ZERO();
        MiniNumber out = MiniNumber::ZERO();

        auto iti = inamounts.find(key);
        if (iti != inamounts.end()) {
            in = iti->second;
        }
        auto ito = outamounts.find(key);
        if (ito != outamounts.end()) {
            out = ito->second;
        }

        MiniNumber difference = out.sub(in);
        diffs.put(key, difference.toString());
    }

    // Assemble
    ret.put("inputs", ins);
    ret.put("outputs", outs);
    ret.put("difference", diffs);

    return ret;
}

org::minima::system::commands::Command* history::getFunction() {
    return new history();
}

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org