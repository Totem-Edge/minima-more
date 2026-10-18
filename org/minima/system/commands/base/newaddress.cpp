#include "org/minima/system/commands/base/newaddress.hpp"

#include <utility>
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

newaddress::newaddress()
    : org::minima::system::commands::Command(
          "newaddress",
          "Create a new address that will not be not used for anything else (not a default change address)") {
}

std::string newaddress::getFullHelp() const {
    return std::string("\nnewaddress\n"
                       "\n"
                       "Create a new address that will not be not used for anything else (not one of the 64 default change address).\n"
                       "\n"
                       "Can be used for a specific use case or for improved privacy.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "newaddress\n");
}

std::unique_ptr<org::minima::utils::json::JSONObject> newaddress::runCommand() {
    // Create base JSON reply
    std::unique_ptr<org::minima::utils::json::JSONObject> ret = getJSONReply();

    // Get the wallet
    org::minima::database::wallet::Wallet& wallet =
        org::minima::database::MinimaDB::getDB()->getWallet();

    // Create a new address - not a default address!
    std::unique_ptr<ScriptRow> srow =
        wallet.createNewSimpleAddress(false);

    // How many keys in total are there
    int keynumber = static_cast<int>(wallet.getAllKeys().size());

    // Build the newkey JSON (replicates ScriptRow.toJSON()) and add "total"
    org::minima::utils::json::JSONObject newkey;
    if (srow) {
        newkey.put("script", srow->getScript());
        newkey.put("address", srow->getAddress());
        newkey.put("simple", srow->isSimple());
        newkey.put("default", srow->isDefault());
        newkey.put("publickey", srow->getPublicKey());
        newkey.put("track", srow->isTrack());
    }
    newkey.put("total", keynumber);

    // Put the details in the response
    ret->put("response", newkey);

    return ret;
}

org::minima::system::commands::Command* newaddress::getFunction() {
    return new newaddress();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org