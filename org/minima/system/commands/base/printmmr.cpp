#include "org/minima/system/commands/base/printmmr.hpp"

#include <any>
#include <memory>
#include <string>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Include full definitions for tree and node to access getTip()/getMMR()
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

printmmr::printmmr()
    : org::minima::system::commands::Command(
          "printmmr",
          "Print the MMR set of the tip block") {
}

std::string printmmr::getFullHelp() const {
    return std::string("\nprintmmr\n")
        + "\n"
        + "Print the MMR set of the tip block and the total number of entries in the MMR.\n"
        + "\n"
        + "Returns the tip block number, latest entrynumber and latest set of MMR entries.\n"
        + ""
        + "For each entry, details of its row, entry number, data and value of all new and updated MMR entries for the tip block.\n"
        + "\n"
        + "Row 1 represents the leaf nodes, entry 0 represents the first entry on a row.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "printmmr\n";
}

std::unique_ptr<org::minima::utils::json::JSONObject> printmmr::runCommand() {
    // Base JSON reply object
    auto ret = getJSONReply();

    // Access the tip MMR
    org::minima::objects::mmr::MMR& mmr =
        org::minima::database::MinimaDB::getDB()
            ->getTxPoWTree()
            .getTip()
            ->getMMR();

    // Add the MMR JSON to the response
    ret->put("response", mmr.toJSON());

    return ret;
}

org::minima::system::commands::Command* printmmr::getFunction() {
    return new printmmr();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org