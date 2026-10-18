#include "org/minima/system/commands/scripts/scripts.hpp"

#include <memory>
#include <utility>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp" // Global ScriptRow with toJSON()
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

scripts::scripts()
    : org::minima::system::commands::Command("scripts", "(address:) - Search scripts / addresses") {
}

std::string scripts::getFullHelp() const {
    return "\nscripts\n"
           "\n"
           "List all scripts or search for a script / basic address your node is tracking.\n"
           "\n"
           "address: (optional)\n"
           "    Script address or basic address to search for. Can be 0x or Mx address.\n"
           "\n"
           "Examples:\n"
           "\n"
           "scripts\n"
           "\n"
           "scripts address:0xFED5..\n"
           "\n"
           "scripts address:MxG087..n";
}

std::vector<std::string> scripts::getValidParams() const {
    return { "address" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> scripts::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::wallet::Wallet;
    using org::minima::system::commands::CommandException;

    auto ret = getJSONReply();

    // Get the wallet
    Wallet& wallet = MinimaDB::getDB()->getWallet();

    // Normalize optional address parameter (handles Mx -> 0x conversion and validation)
    std::string address = getAddressParam("address", "");

    if (address.empty()) {
        // List all the custom scripts
        auto allscripts = wallet.getAllAddresses();

        org::minima::utils::json::JSONArray arr;
        for (const auto& kr : allscripts) {
            // Build a global ScriptRow (with toJSON) from the wallet ScriptRow values
            ::ScriptRow jsonsr(
                kr->getScript(),
                kr->getAddress(),
                kr->isSimple(),
                kr->isDefault(),
                kr->getPublicKey(),
                kr->isTrack()
            );
            arr.add(jsonsr.toJSON());
        }

        ret->put("response", arr);
    } else {
        // Search for that address
        auto scrow = wallet.getScriptFromAddress(address);
        if (!scrow) {
            throw CommandException("Script with that address not found");
        }

        // Convert to JSON via the global ScriptRow helper
        ::ScriptRow jsonsr(
            scrow->getScript(),
            scrow->getAddress(),
            scrow->isSimple(),
            scrow->isDefault(),
            scrow->getPublicKey(),
            scrow->isTrack()
        );
        ret->put("response", jsonsr.toJSON());
    }

    return ret;
}

org::minima::system::commands::Command* scripts::getFunction() {
    return new scripts();
}

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org