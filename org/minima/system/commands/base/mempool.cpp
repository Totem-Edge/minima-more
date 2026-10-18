#include "org/minima/system/commands/base/mempool.hpp"

#include <any>
#include <cstdint>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowdb/ram/ram_data.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

mempool::mempool()
    : org::minima::system::commands::Command("mempool", "Check the mempool") {
}

std::unique_ptr<org::minima::utils::json::JSONObject> mempool::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::txpowdb::TxPoWDB;
    using org::minima::database::txpowdb::ram::RamData;
    using org::minima::objects::TxPoW;
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    // Base reply JSON
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Access TxPoWDB
    TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();

    // Array of txpows
    JSONArray txpows;

    // Full mempool snapshot
    auto mempool = txpdb.getCompleteMemPool();

    int size = 0;
    int cascade = 0;
    int txnnum = 0;
    int blknum = 0;

    // Iterate over all RamData entries
    for (const auto& kv : mempool) {
        const std::shared_ptr<RamData>& ram = kv.second;

        if (!ram->isInCascade()) {
            std::shared_ptr<TxPoW> txp = ram->getTxPoW();

            ++size;

            JSONObject txpow;
            txpow.put("txpowid", std::any(txp->getTxPoWID()));
            txpow.put("transaction", std::any(static_cast<bool>(txp->isTransaction())));
            txpow.put("block", std::any(static_cast<bool>(txp->isBlock())));

            txpows.add(std::any(txpow));

            if (txp->isTransaction()) {
                ++txnnum;
            }
            if (txp->isBlock()) {
                ++blknum;
            }
        } else {
            ++cascade;
        }
    }

    JSONObject resp;
    resp.put("txpow", std::any(txpows));
    resp.put("total", std::any(static_cast<long long>(mempool.size())));
    resp.put("onchain", std::any(size));
    resp.put("cascade", std::any(cascade));
    resp.put("transactions", std::any(txnnum));
    resp.put("blocks", std::any(blknum));

    // Add response
    ret->put("response", std::any(resp));

    return ret;
}

org::minima::system::commands::Command* mempool::getFunction() {
    return new mempool();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org