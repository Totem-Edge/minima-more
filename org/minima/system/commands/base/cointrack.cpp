#include "org/minima/system/commands/base/cointrack.hpp"

#include <utility>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry_number.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::objects::Coin;
using org::minima::objects::base::MiniData;
using org::minima::objects::mmr::MMREntryNumber;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::utils::json::JSONObject;
using org::minima::system::commands::CommandException;

cointrack::cointrack()
    : org::minima::system::commands::Command(
          "cointrack",
          "[enable:true|false] [coinid:] - Track or untrack a coin") {
}

std::string cointrack::getFullHelp() const {
    return std::string("\ncointrack\n"
                       "\n"
                       "Track or untrack a coin.\n"
                       "\n"
                       "Track a coin to keep its MMR proof up-to-date and know when it becomes spent. Stop tracking to remove it from your relevant coins list.\n"
                       "\n"
                       "enable:\n"
                       "    true or false, true will add the coin to your relevant coins, false will remove it from your relevant coins.\n"
                       "\n"
                       "coinid:\n"
                       "    The id of a coin. Can be found using the 'coins' command.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "cointrack enable:true coinid:0xCD34..\n");
}

std::vector<std::string> cointrack::getValidParams() const {
    return std::vector<std::string>{ "enable", "coinid" };
}

std::unique_ptr<JSONObject> cointrack::runCommand() {
    // Prepare reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Params
    const std::string coinid = getParam("coinid");
    const MiniData coindata(coinid);
    const bool track = getBooleanParam("enable");

    // Locate the tree node for this coin
    std::shared_ptr<TxPoWTreeNode> node = TxPoWSearcher::getTreeNodeForCoin(MiniData(coinid));
    if (!node) {
        throw CommandException(std::string("Coin not found coinid : ") + coinid);
    }

    // Find the coin (chain search; MEGAMMR disabled)
    std::shared_ptr<Coin> coin = TxPoWSearcher::searchCoin(coindata, false);

    // Build the MMR entry number shared_ptr required by TxPoWTreeNode API
    MMREntryNumber entryNum = coin->getMMREntryNumber();

    if (track) {
        // Already tracking?
        if (node->isRelevantEntry(entryNum)) {
            ret->put("response", std::string("Coin already tracked"));
        } else {
            // Add to relevant coins and recalculate, matching Java behaviour.
            node->addRelevantCoin(entryNum);
            node->calculateRelevantCoins();

            ret->put("response", std::string("Coin added to track list"));
        }
    } else {
        // Stop tracking and recalculate, matching Java behaviour.
        node->removeRelevantCoin(entryNum);
        node->calculateRelevantCoins();

        ret->put("response", std::string("Coin removed from track list"));
    }

    return ret;
}

org::minima::system::commands::Command* cointrack::getFunction() {
    return new cointrack();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org