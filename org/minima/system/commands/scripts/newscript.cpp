#include "org/minima/system/commands/scripts/newscript.hpp"

#include <memory>
#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"
#include "org/minima/kissvm/contract.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No OS-specific behavior needed currently.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

newscript::newscript()
    : org::minima::system::commands::Command(
          "newscript",
          "[script:] [trackall:false|true] (clean:false|true) - Add a new custom script. Track ALL addresses or just ones with relevant state variables.") {}

std::string newscript::getFullHelp() const {
    return std::string("\nnewscript\n")
        + "\n"
        + "Add a new custom script.\n"
        + "\n"
        + "Track ALL coins with this script address or just ones with state variables relevant to you.\n"
        + "\n"
        + "script:\n"
        + "    The script to add to your node.\n"
        + "    Your node will then know about coins with this script address.\n"
        + "\n"
        + "trackall:\n"
        + "    true or false, true will track all coins with this script address.\n"
        + "    false will only track coins with this script address that are relevant to you.\n"
        + "\n"
        + "clean: (optional)\n"
        + "    true or false, true will clean the script to its minimal correct representation.\n"
        + "    Default is false.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "newscript trackall:true script:\"RETURN SIGNEDBY(0x1539..) AND SIGNEDBY(0xAD25..)\"\n"
        + "\n"
        + "newscript trackall:false script:\"RETURN (@BLOCK GTE PREVSTATE(1) OR @COINAGE GTE PREVSTATE(4)) AND VERIFYOUT(@INPUT PREVSTATE(2) @AMOUNT @TOKENID FALSE)\"\n";
}

std::vector<std::string> newscript::getValidParams() const {
    return std::vector<std::string>{ "script", "trackall", "clean" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> newscript::runCommand() {
    // JSON reply object
    auto ret = getJSONReply();

    // Get the wallet
    org::minima::database::wallet::Wallet& wallet =
        org::minima::database::MinimaDB::getDB()->getWallet();

    // Get parameters
    const std::string script = getParam("script");
    const bool track = getBooleanParam("trackall");
    const bool clean = getBooleanParam("clean", false);

    // Clean the script if requested
    std::string finalscript = script;
    if (clean) {
        finalscript = org::minima::kissvm::Contract::cleanScript(finalscript);
    }

    // Construct Address from script
    org::minima::objects::Address addr(finalscript);

    // Compute address string. Prefer using Minima (Mx) address representation here
    // due to unavailable MiniData hex conversion in provided headers context.
    const std::string address_str = addr.getMinimaAddress();

    // Check if we have this script already
    std::unique_ptr<ScriptRow> existing =
        wallet.getScriptFromAddress(address_str);

    // If we have it, remove it first
    if (existing) {
        wallet.removeScript(address_str);
    }

    // Now add it to the DB
    std::unique_ptr<ScriptRow> added =
        wallet.addScript(finalscript, /*simple=*/false, /*default=*/false, /*publickey=*/"0x00", track);

    // Build the response JSON
    org::minima::utils::json::JSONObject rowjson;
    if (added) {
        rowjson.put("script", added->getScript());
        rowjson.put("address", added->getAddress());
        rowjson.put("simple", static_cast<bool>(added->isSimple()));
        rowjson.put("default", static_cast<bool>(added->isDefault()));
        rowjson.put("publickey", added->getPublicKey());
        rowjson.put("track", static_cast<bool>(added->isTrack()));
    } else {
        // Defensive: if addScript unexpectedly returned null
        rowjson.put("error", std::string("Failed to add script"));
    }

    // Put the details in the response
    ret->put("response", rowjson);

    return ret;
}

org::minima::system::commands::Command* newscript::getFunction() {
    return new newscript();
}

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org