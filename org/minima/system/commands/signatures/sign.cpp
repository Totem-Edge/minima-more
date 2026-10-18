#include "org/minima/system/commands/signatures/sign.hpp"

#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace signatures {

sign::sign()
    : org::minima::system::commands::Command(
          "sign",
          "[publickey:] [data:] - Sign the data with the publickey") {}

std::string sign::getFullHelp() const {
    return
        "\nsign\n"
        "\n"
        "Sign the data with the publickey.\n"
        "\n"
        "Returns the signature of the data, signed with the corresponding private key.\n"
        "\n"
        "data:\n"
        "    The 0x HEX data to sign.\n"
        "\n"
        "Examples:\n"
        "\n"
        "sign data:0xCD34..\n";
}

std::vector<std::string> sign::getValidParams() const {
    return std::vector<std::string>{ "publickey", "data" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> sign::runCommand() {
    auto ret = getJSONReply();

    // Retrieve parameters (throws via Command on missing/invalid)
    std::unique_ptr<org::minima::objects::base::MiniData> data = getDataParam("data");
    std::unique_ptr<org::minima::objects::base::MiniData> pubk = getDataParam("publickey");

    // Access Wallet
    org::minima::database::wallet::Wallet& wallet = org::minima::database::MinimaDB::getDB()->getWallet();

    // Sign the data
    std::unique_ptr<org::minima::objects::keys::Signature> signature =
        wallet.signData(pubk->to0xString(), *data);

    if (!signature) {
        throw std::runtime_error("Failed to sign data: signature is null");
    }

    // Serialize signature into MiniData and return hex string
    std::unique_ptr<org::minima::objects::base::MiniData> sigmd =
        org::minima::objects::base::MiniData::getMiniDataVersion(*signature);

    if (!sigmd) {
        throw std::runtime_error("Failed to serialize signature to MiniData");
    }

    ret->put("response", sigmd->to0xString());
    return ret;
}

org::minima::system::commands::Command* sign::getFunction() {
    return new sign();
}

} // namespace signatures
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org