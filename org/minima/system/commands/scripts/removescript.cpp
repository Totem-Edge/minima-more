#include "org/minima/system/commands/scripts/removescript.hpp"

#include <memory>
#include <string>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

removescript::removescript()
    : org::minima::system::commands::Command(
          "removescript",
          "[address:] - Remove a script from your DB") {
}

std::string removescript::getFullHelp() const {
    return "\removescript\n"
           "\n"
           "Remove a custom script. BE CAREFUL not to remove a script you need.\n"
           "\n"
           "address:\n"
           "    The address of the script.\n"
           "\n"
           "Examples:\n"
           "\n"
           "removescript address:0xFFE678768CDE.."
           "\n"
           "removescript address:MxFFE678768CDE..";
}

std::vector<std::string> removescript::getValidParams() const {
    return std::vector<std::string>{ "address" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> removescript::runCommand() {
    // Prepare reply
    auto ret = getJSONReply();

    // Get the wallet
    org::minima::database::wallet::Wallet& wallet =
        org::minima::database::MinimaDB::getDB()->getWallet();

    // Get the address (normalized if necessary)
    std::string address = getAddressParam("address");

    // Remove it
    wallet.removeScript(address);

    // Put the details in the response
    ret->put("response", std::string("Script removed"));

    return ret;
}

org::minima::system::commands::Command* removescript::getFunction() {
    return new removescript();
}

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org