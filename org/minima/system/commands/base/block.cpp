#include "org/minima/system/commands/base/block.hpp"

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/tx_po_w.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

block::block()
    : org::minima::system::commands::Command("block", "Simply return the current top block") {}

std::string block::getFullHelp() const {
    return "\nblock\n"
           "\n"
           "Return the top block\n"
           "\n"
           "Examples:\n"
           "\n"
           "block\n";
}

std::unique_ptr<org::minima::utils::json::JSONObject> block::runCommand() {
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    // Get the top block..
    auto* db = org::minima::database::MinimaDB::getDB();
    org::minima::database::txpowtree::TxPowTree& tree = db->getTxPoWTree();
    std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode> tip = tree.getTip();
    if (!tip) {
        throw org::minima::system::commands::CommandException("NO Blocks yet..");
    }

    // Get the top block
    const org::minima::objects::TxPoW& topblock = tip->getTxPoW();

    JSONObject resp;
    resp.put("block", topblock.getBlockNumber().toString());
    resp.put("hash", topblock.getTxPoWID());
    resp.put("timemilli", topblock.getTimeMilli().toString());

    // Convert milliseconds since epoch to a date string similar to Java Date.toString()
    long long millis = topblock.getTimeMilli().getAsLong();
    using namespace std::chrono;
    system_clock::time_point tp = system_clock::time_point(milliseconds(millis));
    std::time_t tt = system_clock::to_time_t(tp);

    std::tm tmval{};
#ifdef _WIN32
    localtime_s(&tmval, &tt);
#else
    localtime_r(&tt, &tmval);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmval, "%a %b %d %H:%M:%S %Z %Y");
    resp.put("date", oss.str());

    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* block::getFunction() {
    return new block();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org