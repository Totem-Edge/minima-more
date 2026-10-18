#include "org/minima/system/commands/base/getaddress.hpp"

#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/search/keys.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

getaddress::getaddress()
    : org::minima::system::commands::Command("getaddress",
          "Get one of your default Minima addresses") {}

std::string getaddress::getFullHelp() const {
    return std::string("\ngetaddress\n"
                       "\n"
                       "Returns an existing default Minima address to receive funds, use as a change address etc.\n"
                       "\n"
                       "Each address can be used securely 262144 (64^3) times.\n"
                       "\n"
                       "Then you can wipe the private keys from your online node using the 'vault' command.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "getaddress\n");
}

std::vector<std::string> getaddress::getValidParams() const {
    // No valid parameters (matches Java: new ArrayList<>(Arrays.asList(new String[]{})))
    return {};
}

std::unique_ptr<org::minima::utils::json::JSONObject> getaddress::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::wallet::Wallet;

    // JSON reply holder
    auto ret = getJSONReply();

    // Get the wallet
    Wallet& wallet = MinimaDB::getDB()->getWallet();

    // Are we creating them all.. (Java block is commented out - preserve behavior: do nothing)
    if (existsParam("createall")) {
        // Intentionally left blank to mirror Java's commented-out functionality.
        // ret remains without a "response" field in this branch.
    } else {
        // Read the 'type' param (unused in Java but retrieved)
        const std::string type = getParam("type", "single");
        (void)type; // suppress unused variable warning

        // Get an existing address
        std::unique_ptr<ScriptRow> scrow = wallet.getDefaultAddress();

        // Get the key row.. THIS is a fix for an issue where backup saved with wrong seed phrase
        if (MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
            if (!org::minima::system::commands::search::keys::checkKey(scrow->getPublicKey())) {
                throw org::minima::system::commands::CommandException(
                    std::string("[!] SERIOUS ERROR - INCORRECT Public key : ") + scrow->getPublicKey());
            }
        }

        // Put the details in the response..
        // Pitfall 3 bridging: cast the wallet::ScriptRow to the global ::ScriptRow to call toJSON()
        ::ScriptRow* scrow_real = reinterpret_cast<::ScriptRow*>(scrow.get());
        org::minima::utils::json::JSONObject scjson = scrow_real->toJSON();
        ret->put("response", scjson);
    }

    return ret;
}

org::minima::system::commands::Command* getaddress::getFunction() {
    return new getaddress();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org