#include "org/minima/system/commands/base/coinexport.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp" // for getTip()

#include "org/minima/system/brains/tx_po_w_searcher.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"

#include "org/minima/system/params/general_params.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::database::MinimaDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMRProof;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::utils::json::JSONObject;

namespace {
// Helper to normalize various possible return types to std::shared_ptr<T>
template<typename T>
std::shared_ptr<T> to_shared_ptr(std::shared_ptr<T> p) {
    return p;
}
template<typename T>
std::shared_ptr<T> to_shared_ptr(std::unique_ptr<T> p) {
    return std::shared_ptr<T>(std::move(p));
}
template<typename T>
std::shared_ptr<T> to_shared_ptr(const T& v) {
    return std::make_shared<T>(v);
}
} // anonymous namespace

coinexport::coinexport()
    : org::minima::system::commands::Command("coinexport", "[coinid:] - Export a coin") {
}

std::string coinexport::getFullHelp() const {
    return
        "\ncoinexport\n"
        "\n"
        "Export a coin including its MMR proof.\n"
        "\n"
        "A coin can then be imported and tracked on another node using the 'coinimport' command.\n"
        "\n"
        "This does not allow the spending of a coin - just the knowledge of its existence.\n"
        "\n"
        "coinid:\n"
        "    The id of a coin. Can be found using the 'coins' command.\n"
        "\n"
        "Examples:\n"
        "\n"
        "coinexport coinid:0xCD34..\n";
}

std::vector<std::string> coinexport::getValidParams() const {
    return std::vector<std::string>{ "coinid" };
}

std::unique_ptr<JSONObject> coinexport::runCommand() {
    // Prepare base reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get the coin ID parameter
    const std::string id = getParam("coinid");

    // Find the coin
    std::shared_ptr<Coin> coin = TxPoWSearcher::searchCoin(MiniData(id));
    if (!coin) {
        throw org::minima::system::commands::CommandException("Coin not found coinid : " + id);
    }

    // When was it made
    MiniNumber created = coin->getBlockCreated();

    // Get the chain tip
    auto* db = MinimaDB::getDB();
    auto& tree = db->getTxPoWTree();
    std::shared_ptr<TxPoWTreeNode> tip = tree.getTip();

    // How far back shall we go
    MiniNumber history(256);
    if (org::minima::system::params::GeneralParams::TEST_PARAMS) {
        history = MiniNumber(8);
    }

    // Compute the back block number, ensuring it's not earlier than the coin creation
    MiniNumber tipBlockNum = tip->getBlockNumber();
    MiniNumber back = tipBlockNum.sub(history);
    if (back.isLess(created)) {
        back = created;
    }

    // Get the node at that block
    std::shared_ptr<TxPoWTreeNode> mmrnode = tip->getPastNode(back);

    // Get the MMR proof for this coin
    auto& mmr = mmrnode->getMMR();
    auto proof_any = mmr.getProofToPeak(coin->getMMREntryNumber());
    std::shared_ptr<MMRProof> proof = to_shared_ptr<MMRProof>(std::move(proof_any));

    // Create the CoinProof
    CoinProof cp(coin, proof);

    // Create the MiniData version
    std::unique_ptr<MiniData> dataproof = MiniData::getMiniDataVersion(cp);

    // Build the response JSON
    JSONObject resp;
    resp.put("coinproof", cp.toJSON());
    resp.put("data", dataproof ? std::any(dataproof->to0xString()) : std::any(std::string("")));

    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* coinexport::getFunction() {
    return new coinexport();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org