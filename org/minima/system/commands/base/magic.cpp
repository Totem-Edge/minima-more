#include "org/minima/system/commands/base/magic.hpp"

#include <memory>
#include <utility>

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/magic.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::utils::json::JSONObject;
using org::minima::database::MinimaDB;
using org::minima::database::userprefs::UserDB;

magic::magic()
    : org::minima::system::commands::Command(
          "magic",
          "(kissvm:) (txpowsize:) (txnsperblock:) - Set the Magic numbers that define the Minima network overall capacity") {
}

std::vector<std::string> magic::getValidParams() const {
    return std::vector<std::string>{ "kissvm", "txpowsize", "txnsperblock" };
}

std::unique_ptr<JSONObject> magic::runCommand() {
    // Base return object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Response object
    JSONObject resp;

    // Access the UserDB
    UserDB& udb = MinimaDB::getDB()->getUserDB();

    // Set desired KISSVM if provided
    if (existsParam("kissvm")) {
        auto kissvm = getNumberParam("kissvm");
        udb.setMagicDesiredKISSVM(*kissvm);
    }

    // Set desired Max TxPoW size if provided
    if (existsParam("txpowsize")) {
        auto txpowsize = getNumberParam("txpowsize");
        udb.setMagicMaxTxPoWSize(*txpowsize);
    }

    // Set desired max transactions per block if provided
    if (existsParam("txnsperblock")) {
        auto txns = getNumberParam("txnsperblock");
        udb.setMagicMaxTxns(*txns);
    }

    // Get the Tip.. and add the Magic JSON
    auto& txptree = MinimaDB::getDB()->getTxPoWTree();
    auto tip = txptree.getTip(); // expected to be std::shared_ptr<TxPoWTreeNode>
    // Mirror Java behavior; assume tip exists
    JSONObject lastblock = tip->getTxPoW().getMagic().toJSON();
    resp.put("lastblock", lastblock);

    // Desired values
    JSONObject desired;
    desired.put("kissvm", udb.getMagicDesiredKISSVM());
    desired.put("txpowsize", udb.getMagicMaxTxPoWSize());
    desired.put("txnsperblock", udb.getMagicMaxTxns());
    resp.put("desired", desired);

    // Add response to the return JSON
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* magic::getFunction() {
    return new magic();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org