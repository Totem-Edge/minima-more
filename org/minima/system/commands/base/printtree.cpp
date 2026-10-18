#include "org/minima/system/commands/base/printtree.hpp"

#include <stdexcept>
#include <cstdlib>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Include full class definitions to call member functions (Pitfall 12)
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

#ifdef _WIN32
// No OS-specific behavior needed here, but block reserved for future use.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::utils::json::JSONObject;
using org::minima::database::MinimaDB;

printtree::printtree()
    : org::minima::system::commands::Command(
          "printtree",
          "(depth:) (cascade:true|false) - Print a tree representation of the blockchain. Depth default 32, Cascade false.") {
}

std::string printtree::getFullHelp() const {
    return
        "\nprinttree\n"
        "\n"
        "Print a tree representation of the blockchain.\n"
        "\n"
        "Default depth 32 blocks, can be increased to see more of the txpow tree.\n"
        "\n"
        "Optionally show the cascading chain, default is false.\n"
        "\n"
        "depth: (optional)\n"
        "    Number of blocks back from the tip to show in the txpow tree.\n"
        "\n"
        "cascade: (optional)\n"
        "    true or false, true shows the cascade.\n"
        "\n"
        "Examples:\n"
        "\n"
        "printtree\n"
        "\n"
        "printtree depth:500\n"
        "\n"
        "printtree cascade:true\n";
}

std::vector<std::string> printtree::getValidParams() const {
    return {"depth", "cascade"};
}

std::unique_ptr<JSONObject> printtree::runCommand() {
    // Create base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Build response content
    JSONObject resp;

    // depth param with default "32"
    const std::string depthstr = getParam("depth", "32");
    int depth = 0;
    // Parse like Java's Integer.parseInt (throws on invalid input)
    depth = std::stoi(depthstr);

    // cascade param with default "false"
    const bool casc = (getParam("cascade", "false") == "true");

    if (casc) {
        std::string cascade = MinimaDB::getDB()->getCascade().printCascade();
        resp.put("cascade", std::string("\n") + cascade);
    }

    std::string treestr = MinimaDB::getDB()->getTxPoWTree().printTree(depth);
    resp.put("chain", std::string("\n") + treestr);

    // Attach response
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* printtree::getFunction() {
    return new printtree();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org