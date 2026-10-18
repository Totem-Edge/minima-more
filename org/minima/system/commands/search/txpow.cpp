#include "org/minima/system/commands/search/txpow.hpp"

#include <algorithm>
#include <unordered_set>

#ifdef _WIN32
// Windows-specific headers if needed later
#include <windows.h>
#endif

// Minima includes
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/database/txpowdb/onchain/tx_po_w_on_chain_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"

#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/params/general_params.hpp"

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::objects::TxPoW;
using org::minima::objects::TxBlock;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::database::MinimaDB;
using org::minima::database::txpowdb::TxPoWDB;
using org::minima::database::txpowdb::sql::TxPoWSqlDB;
using org::minima::database::txpowdb::onchain::TxPoWOnChainDB;
using org::minima::database::txpowtree::TxPowTree;
using org::minima::database::txpowtree::TxPoWTreeNode;

txpow::txpow()
    : org::minima::system::commands::Command(
          "txpow",
          "(txpowid:) (onchain:) (block:) (address:) (relevant:) (max:) - Search for a specific TxPoW or check for onchain") {}

std::string txpow::getFullHelp() const {
    return std::string()
        + "\ntxpow\n"
        + "\n"
        + "Search for a specific TxPoW in the unpruned chain or your mempool.\n"
        + "\n"
        + "Search by txpowid, block or 0x / Mx address.\n"
        + "\n"
        + "txpowid: (optional)\n"
        + "    TxPoW id of the TxPoW to search for.\n"
        + "    Returns the txpow details.\n"
        + "\n"
        + "onchain: (optional)\n"
        + "    TxPoW id to search for on chain. Must be in the unpruned chain.\n"
        + "    Returns block info and number of confirmations.\n"
        + "\n"
        + "block: (optional)\n"
        + "    Block number to search in. Must be in the unpruned chain.\n"
        + "\n"
        + "address: (optional)\n"
        + "    0x or Mx address. Search for TxPoWs containing this specific address.\n"
        + "\n"
        + "relevant: (optional)\n"
        + "    true or false. Only list TxPoWs relevant to this node.\n"
        + "\n"
        + "max: (optional)\n"
        + "    Max relevant TxPoW to retrieve. Default 100.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "txpow txpowid:0x000..\n"
        + "\n"
        + "txpow block:200\n"
        + "\n"
        + "txpow address:0xCEF6..\n"
        + "\n"
        + "txpow onchain:0x000..\n";
}

std::vector<std::string> txpow::getValidParams() const {
    return std::vector<std::string>{
        "txpowid","block","address","onchain","relevant","max","action","inblock"
    };
}

static std::string any_to_string(const std::any& v) {
    if (!v.has_value()) return "";
    if (v.type() == typeid(std::string)) return std::any_cast<std::string>(v);
    if (v.type() == typeid(const char*)) return std::string(std::any_cast<const char*>(v));
    if (v.type() == typeid(bool)) return std::any_cast<bool>(v) ? "true" : "false";
    if (v.type() == typeid(int)) return std::to_string(std::any_cast<int>(v));
    if (v.type() == typeid(long long)) return std::to_string(std::any_cast<long long>(v));
    if (v.type() == typeid(double)) return std::to_string(std::any_cast<double>(v));
    // Fallback for JSONObject / JSONArray is to rely on their toString if needed by caller
    return "";
}

std::unique_ptr<JSONObject> txpow::runCommand() {
    auto ret = getJSONReply();

    // action
    if (existsParam("action")) {
        std::string action = getParam("action");
        if (action == "info") {
            JSONObject txdb;
            int txpowdbsize = MinimaDB::getDB()->getTxPoWDB().getSqlSize();
            txdb.put("size", txpowdbsize);

            TxPoWOnChainDB* ocdb = MinimaDB::getDB()->getTxPoWDB().getOnChainDB();
            JSONObject onchaindb;
            onchaindb.put("size", ocdb->getSize());
            onchaindb.put("first", ocdb->getFirstTxPoW());
            onchaindb.put("last", ocdb->getLastTxPoW());

            JSONObject infojson;
            infojson.put("storedays", static_cast<long long>(org::minima::system::params::GeneralParams::NUMBER_DAYS_SQLTXPOWDB));
            infojson.put("txpowdb", txdb);
            infojson.put("onchaindb", onchaindb);

            ret->put("response", infojson);
        } else {
            throw org::minima::system::commands::CommandException(std::string("Invalid action : ") + action);
        }

    } else if (existsParam("txpowid")) {
        std::string txpowid = getAddressParam("txpowid");

        // 1) Search TxPoW DB / mempool
        std::shared_ptr<TxPoW> txpow_found;
        {
            // TxPoWDB may return different ownership; handle robustly
            auto* txpdb = &MinimaDB::getDB()->getTxPoWDB();
            // Prefer a method that returns a shared_ptr if available; otherwise wrap
            // Assume interface: std::shared_ptr<TxPoW> getTxPoW(const std::string&)
            txpow_found = txpdb->getTxPoW(txpowid);
        }

        if (!txpow_found) {
            // 2) Check the archive by txpowid
            auto* arch = &MinimaDB::getDB()->getArchive();
            // Assume interface returns std::shared_ptr<TxBlock> or std::unique_ptr<TxBlock>
            std::shared_ptr<TxBlock> blockptr = arch->loadBlock(txpowid);
            if (blockptr) {
                // We can serialize directly to JSON
                ret->put("response", blockptr->getTxPoW().toJSON());
                return ret;
            } else {
                // 3) Check MySQL if auto-login enabled
                bool autologindetail = MinimaDB::getDB()->getUserDB().getAutoLoginDetailsMySQL();
                if (autologindetail) {
                    std::string command = std::string("mysql action:findtxpow txpowid:") + txpowid;
                    auto res = org::minima::system::commands::CommandRunner::getRunner()->runMultiCommand(command);
                    if (!res || res->size() == 0) {
                        throw org::minima::system::commands::CommandException(std::string("TxPoW not found : ") + txpowid);
                    }

                    const std::any& first = res->at(0);
                    JSONObject resultObj;
                    if (first.type() == typeid(JSONObject)) {
                        resultObj = std::any_cast<JSONObject>(first);
                    } else if (first.type() == typeid(std::shared_ptr<JSONObject>)) {
                        auto pobj = std::any_cast<std::shared_ptr<JSONObject>>(first);
                        if (pobj) resultObj = *pobj;
                    } else {
                        // Fallback: cannot parse
                        throw org::minima::system::commands::CommandException(std::string("TxPoW not found : ") + txpowid);
                    }

                    const std::any& respany = resultObj.get("response");
                    JSONObject responseObj;
                    if (respany.type() == typeid(JSONObject)) {
                        responseObj = std::any_cast<JSONObject>(respany);
                    } else if (respany.type() == typeid(std::shared_ptr<JSONObject>)) {
                        auto pobj = std::any_cast<std::shared_ptr<JSONObject>>(respany);
                        if (pobj) responseObj = *pobj;
                    } else {
                        throw org::minima::system::commands::CommandException(std::string("TxPoW not found : ") + txpowid);
                    }

                    bool found = false;
                    if (responseObj.containsKey("found")) {
                        found = responseObj.getBoolean("found");
                    }
                    if (found) {
                        // responseObj.get("txpow") could be JSONObject
                        ret->put("response", responseObj.get("txpow"));
                        return ret;
                    } else {
                        throw org::minima::system::commands::CommandException(std::string("TxPoW not found : ") + txpowid);
                    }
                } else {
                    throw org::minima::system::commands::CommandException(std::string("TxPoW not found : ") + txpowid);
                }
            }
        }

        // Found in mempool/db
        ret->put("response", txpow_found->toJSON());

    } else if (existsParam("relevant")) {

        int max = getNumberParam("max", TxPoWSqlDB::MAX_RELEVANT_TXPOW)->getAsInt();

        // Retrieve relevant TxPoWs
        auto* sql = MinimaDB::getDB()->getTxPoWDB().getSQLDB();
        auto vec = sql->getAllRelevant(max);

        JSONArray txns;
        std::unordered_set<std::string> added;
        for (auto& uptr : vec) {
            if (!uptr) continue;
            std::string txpowid = uptr->getTxPoWID();
            if (added.insert(txpowid).second) {
                txns.add(uptr->toJSON());
            }
        }
        ret->put("response", txns);

    } else if (existsParam("inblock")) {

        TxPoWOnChainDB* chaindb = MinimaDB::getDB()->getTxPoWDB().getOnChainDB();
        JSONObject resp;
        std::shared_ptr<JSONArray> txns_ptr;
        JSONArray onchaintxpow;

        std::string inb = getParam("inblock");
        if (!inb.empty() && inb.rfind("0x", 0) == 0) {
            auto txpowid = getDataParam("inblock");
            onchaintxpow = chaindb->getInBlockTxPoW(txpowid->to0xString());
        } else {
            auto block = getNumberParam("inblock");
            onchaintxpow = chaindb->getInBlockTxPoW(block->getAsLong());
        }

        resp.put("txns", onchaintxpow);
        ret->put("response", resp);

    } else if (existsParam("onchain")) {

        auto txpowid = getDataParam("onchain");

        JSONObject resp;

        auto block = org::minima::system::brains::TxPoWSearcher::searchChainForTxPoW(*txpowid);
        if (!block) {
            TxPoWOnChainDB* chaindb = MinimaDB::getDB()->getTxPoWDB().getOnChainDB();
            JSONObject onchaintxpow = chaindb->getOnChainTxPoW(txpowid->to0xString());

            bool found = false;
            if (onchaintxpow.containsKey("found")) {
                found = onchaintxpow.getBoolean("found");
            }

            if (found) {
                // Block number comes back as a value stored in std::any; convert to string then MiniNumber
                std::string blockStr = any_to_string(onchaintxpow.get("block"));
                MiniNumber fblock(blockStr);

                // Tip node
                auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

                resp.put("found", true);
                resp.put("block", fblock.toString());
                resp.put("blockid", any_to_string(onchaintxpow.get("blockid")));
                resp.put("tip", tip->getBlockNumber().toString());

                MiniNumber depth = tip->getBlockNumber().sub(fblock);
                resp.put("confirmations", depth.toString());
            } else {
                resp.put("found", false);
            }

        } else {
            auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

            resp.put("found", true);
            resp.put("block", block->getBlockNumber().toString());
            resp.put("blockid", block->getTxPoWID());
            resp.put("tip", tip->getBlockNumber().toString());

            MiniNumber depth = tip->getBlockNumber().sub(block->getBlockNumber());
            resp.put("confirmations", depth.toString());
        }

        ret->put("response", resp);

    } else if (existsParam("block")) {

        auto blocknum = getNumberParam("block");

        auto txpowblk = org::minima::system::brains::TxPoWSearcher::getTxPoWBlock(*blocknum);
        if (!txpowblk) {
            auto* arch = &MinimaDB::getDB()->getArchive();
            auto txblock = arch->loadBlockFromNumber(*blocknum);
            if (!txblock) {
                throw org::minima::system::commands::CommandException(
                    std::string("TxPoW not found @ height ") + blocknum->toString());
            }
            ret->put("response", txblock->getTxPoW().toJSON());
        } else {
            ret->put("response", txpowblk->toJSON());
        }

    } else if (existsParam("address")) {

        std::string address = getAddressParam("address");

        auto txps = org::minima::system::brains::TxPoWSearcher::searchTxPoWviaAddress(MiniData(address));

        JSONArray txns;
        std::unordered_set<std::string> added;
        for (auto& txp : txps) {
            if (!txp) continue;
            std::string txpowid = txp->getTxPoWID();
            if (added.insert(txpowid).second) {
                txns.add(txp->toJSON());
            }
        }

        ret->put("response", txns);

    } else {
        throw org::minima::system::commands::CommandException("Must Specify search params");
    }

    return ret;
}

org::minima::system::commands::Command* txpow::getFunction() {
    return new txpow();
}

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org