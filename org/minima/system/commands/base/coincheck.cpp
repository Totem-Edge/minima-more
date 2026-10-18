#include "org/minima/system/commands/base/coincheck.hpp"

#include <memory>
#include <utility>

// Project includes
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

// Minimal forward declarations for methods we call (to avoid depending on unavailable headers)
namespace org { namespace minima { namespace objects { namespace mmr {
    class MMRProof {
    public:
        // Needed by this translation unit
        long long getBlockTime() const;
    };

    class MMR {
    public:
        // Needed by this translation unit
        bool checkProofTimeValid(
            const org::minima::objects::mmr::MMREntryNumber&,
            const org::minima::objects::mmr::MMRData&,
            const org::minima::objects::mmr::MMRProof&) const;
    };
} } } }

namespace org { namespace minima { namespace database { namespace txpowtree {
    class TxPowTree {
    public:
        // Needed by this translation unit
        std::shared_ptr<TxPoWTreeNode> getTip();
    };
} } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

coincheck::coincheck()
    : org::minima::system::commands::Command("coincheck", "[data:] - Check a coin exists") {
}

std::string coincheck::getFullHelp() const {
    return std::string("\ncoincheck\n")
        + "\n"
        + "Check a coin exists and is valid. Can only check unspent coins.\n"
        + "\n"
        + "Returns the coin details and whether the MMR proof is valid.\n"
        + "\n"
        + "data:\n"
        + "    The data of a coin. Can be found using the 'coinexport' command.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "coincheck data:0x00000..\n";
}

std::vector<std::string> coincheck::getValidParams() const {
    return std::vector<std::string>{ "data" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> coincheck::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::objects::Coin;
    using org::minima::objects::CoinProof;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::mmr::MMRData;
    using org::minima::utils::json::JSONObject;

    // Base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get data param (throws CommandException in base if missing/blank)
    std::string data = getParam("data");

    // Convert to a CoinProof from MiniData
    MiniData md(data);
    std::unique_ptr<CoinProof> newcoinproof = CoinProof::convertMiniDataVersion(md);
    if (!newcoinproof) {
        throw org::minima::system::commands::CommandException("Invalid coin proof data");
    }

    // Access coin
    Coin& newcoin = newcoinproof->getCoin();

    // Check is UNSPENT
    if (newcoin.getSpent()) {
        throw org::minima::system::commands::CommandException("Coin is spent. Can only check UNSPENT coins.");
    }

    // Get the tip node
    auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

    // Get the Coin for MMR calculation
    Coin& txcoin = newcoinproof->getCoin();

    // Create the MMRData Leaf Node
    std::unique_ptr<MMRData> mmrcoin = MMRData::CreateMMRDataLeafNode(txcoin, txcoin.getAmount());

    // Check the MMR proof
    bool validmmr = tip->getMMR().checkProofTimeValid(
        newcoinproof->getCoin().getMMREntryNumber(),
        *mmrcoin,
        newcoinproof->getMMRProof()
    );

    // Build response
    JSONObject resp;
    resp.put("proofblock", newcoinproof->getMMRProof().getBlockTime());
    resp.put("coin", newcoin.toJSON());
    resp.put("valid", validmmr);

    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* coincheck::getFunction() {
    return new coincheck();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org