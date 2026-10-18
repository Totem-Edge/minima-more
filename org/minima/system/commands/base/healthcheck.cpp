#include "org/minima/system/commands/base/healthcheck.hpp"

#include <stdexcept>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::database::MinimaDB;
using org::minima::database::txpowtree::TxPowTree; // Added TxPowTree
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::cascade::Cascade;
using org::minima::database::cascade::CascadeNode;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::json::JSONObject;

healthcheck::healthcheck()
    : org::minima::system::commands::Command("healthcheck",
          "Run a system check to see everything adds up") {}

std::string healthcheck::getFullHelp() const {
    return std::string("\nhealthcheck\n")
        + "\n"
        + "Return information about your chain and cascade.\n"
        + "\n"
        + "Chain - tip:current chain tip block, root:current chain root block, chainlength:number of blocks in the heaviest chain.\n"
        + "\n"
        + "Cascade - tip:current cascade tip block, tipcorrect:returns true if the cascade tip meets the root of the txpow tree.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "healthcheck\n";
}

std::unique_ptr<JSONObject> healthcheck::runCommand() {
    std::unique_ptr<JSONObject> ret = getJSONReply();
    JSONObject resp;

    // First get the tip..
    auto* db = MinimaDB::getDB();
    auto& tree = db->getTxPoWTree();
    auto tip = tree.getTip();
    if (!tip) {
        throw org::minima::system::commands::CommandException("No TIP block found.. ?");
    }

    JSONObject chain;

    // 1. Declare your POINTER variables ONCE in the outer scope
    std::shared_ptr<MiniNumber> tipbn = std::make_shared<MiniNumber>(tip->getBlockNumber());
    std::shared_ptr<MiniNumber> treeroot = nullptr; // Initialize as null

    // Tip block number as string
    // 'tipbn' is a pointer, so use ->
    chain.put("tip", tipbn->toString());

    // Root and chain length
    auto root = tree.getRoot();
    if (root) {
        // 2. ASSIGN to the existing pointer, don't redeclare
        treeroot = std::make_shared<MiniNumber>(root->getBlockNumber());
        chain.put("root", treeroot->toString());
    } else {
        chain.put("root", std::string(""));
    }

    // 3. This 'if' check now compares two shared_ptrs, which is VALID
    if (tipbn && treeroot) {
        MiniNumber len = tipbn->sub(*treeroot); // Use -> and *
        chain.put("chainlength", len.toString());
    } else {
        chain.put("chainlength", std::string(""));
    }
    
    resp.put("chain", chain);

    // Now check the cascade
    {
        Cascade& casc = db->getCascade();
        auto ctip = casc.getTip();
        if (ctip != nullptr) {
            // Cascade tip block number (MiniNumber value)
            MiniNumber casctip = ctip->getTxPoW().getBlockNumber();

            JSONObject cascade;
            cascade.put("tip", casctip.toString());

            // Compare with root-1
            bool correctstart = false;
            
            // 4. This 'if' check now uses the 'treeroot' POINTER from the outer scope
            if (treeroot) {
                correctstart = casctip.isEqual(treeroot->decrement());
            }
            cascade.put("tipcorrect", correctstart);
            cascade.put("cascadelength", casc.getLength());
            resp.put("cascade", cascade);
        } else {
            resp.put("cascade", std::string("nocacade"));
        }
    }

    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* healthcheck::getFunction() {
    return new healthcheck();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
