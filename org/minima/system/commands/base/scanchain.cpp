#include "org/minima/system/commands/base/scanchain.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/transaction.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/system/commands/command_exception.hpp"

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

namespace {
static std::string format_date_from_millis(long long ms) {
    using namespace std::chrono;
    system_clock::time_point tp = system_clock::time_point(milliseconds(ms));
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    // Approximate Java Date.toString()
    oss << std::put_time(&tm, "%a %b %d %H:%M:%S %Y %Z");
    return oss.str();
}
} // anonymous namespace

scanchain::scanchain()
    : org::minima::system::commands::Command(
          "scanchain",
          "(depth:) - Scan back through the chain and see all transaction data") {}

std::vector<std::string> scanchain::getValidParams() const {
    return std::vector<std::string>{"depth"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> scanchain::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::txpowtree::TxPowTree;
    using org::minima::database::txpowtree::TxPoWTreeNode;
    using org::minima::objects::TxPoW;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;
    using org::minima::system::commands::CommandException;

    // Base reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get the top block (tip)
    TxPowTree& tree = MinimaDB::getDB()->getTxPoWTree();
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();
    if (!tip) {
        throw CommandException("NO Blocks yet..");
    }

    MiniNumber startblock = tip->getBlockNumber();

    // Depth param (default 16)
    int depth = 0;
    {
        std::unique_ptr<MiniNumber> d = getNumberParam("depth", MiniNumber(16));
        depth = d->getAsInt();
    }

    // Traverse back through the chain
    JSONArray blockdata;
    int counter = 0;
    while (tip && counter <= depth) {
        TxPoW& topblock = tip->getTxPoW();

        JSONObject blockjson;
        // Block number
        blockjson.put("block", topblock.getBlockNumber());
        // Depth from start
        blockjson.put("depth", startblock.sub(topblock.getBlockNumber()));
        // Time milli and date string
        MiniNumber timemilli = topblock.getTimeMilli();
        blockjson.put("timemilli", timemilli);
        blockjson.put("date", format_date_from_millis(timemilli.getAsLong()));
        // TxPoWID
        blockjson.put("txpowid", topblock.getTxPoWID());

        // All the transaction data in the block
        JSONArray transactiondata;

        // Is this block a transaction
        if (topblock.isTransaction()) {
            auto txjson = getTransactionDetails(topblock);
            transactiondata.add(*txjson);
        }

        // Add all the transactions referenced by this block
        std::vector<MiniData> alltrans = topblock.getBlockTransactions();
        auto& txpowdb = org::minima::database::MinimaDB::getDB()->getTxPoWDB();
        for (const auto& txid : alltrans) {
            // Referenced transactions live in the TxPoW DB, not necessarily the block tree.
            auto txpow = txpowdb.getTxPoW(txid.to0xString());
            if (txpow) {
                auto txjson = getTransactionDetails(*txpow);
                transactiondata.add(*txjson);
            }
        }

        blockjson.put("transactions", transactiondata);

        // Add to final list
        blockdata.add(blockjson);

        // Move to the next block (parent)
        tip = tip->getParent();
        ++counter;
    }

    JSONObject resp;
    resp.put("depth", depth);
    resp.put("blocks", blockdata);
    ret->put("response", resp);

    return ret;
}

std::unique_ptr<org::minima::utils::json::JSONObject>
scanchain::getTransactionDetails(const org::minima::objects::TxPoW& zTxPoW) const {
    using org::minima::utils::json::JSONObject;

    auto ret = std::make_unique<JSONObject>();
    ret->put("txpowid", zTxPoW.getTxPoWID());

    // transaction
    bool istransaction = false;
    try {
        istransaction = !zTxPoW.getTransaction().isEmpty();
    } catch (const std::exception&) {
        istransaction = false;
    }
    ret->put("istransaction", istransaction);
    if (istransaction) {
        ret->put("transaction", zTxPoW.getTransaction().toJSON());
    }

    // burn transaction
    bool isburntransaction = false;
    try {
        isburntransaction = !zTxPoW.getBurnTransaction().isEmpty();
    } catch (const std::exception&) {
        isburntransaction = false;
    }
    ret->put("isburntransaction", isburntransaction);
    if (isburntransaction) {
        ret->put("burntransaction", zTxPoW.getBurnTransaction().toJSON());
    }

    return ret;
}

org::minima::system::commands::Command* scanchain::getFunction() {
    return new scanchain();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
