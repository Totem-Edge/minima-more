#include "org/minima/system/commands/signatures/verify.hpp"

#include <stdexcept>
#include <utility>

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace signatures {

using org::minima::objects::base::MiniData;
using org::minima::objects::keys::Signature;
using org::minima::objects::keys::TreeKey;
using org::minima::utils::json::JSONObject;
using org::minima::system::commands::CommandException;

verify::verify()
    : org::minima::system::commands::Command(
          "verify", "[publickey:] [data:] [signature:] - Verify a signature") {}

std::string verify::getFullHelp() const {
    return
        "\nverify\n"
        "\n"
        "Verify a signature. Returns valid true or false.\n"
        "\n"
        "data:\n"
        "    The 0x HEX data to verify the signature for.\n"
        "\n"
        "publickey:\n"
        "    The public key of the signer.\n"
        "\n"
        "signature:\n"
        "    The signature of the data.\n"
        "\n"
        "Examples:\n"
        "\n"
        "verify data:0xCD34.. publickey:0xFED5 signature:0x4827..\n";
}

std::vector<std::string> verify::getValidParams() const {
    return std::vector<std::string>{ "publickey", "data", "signature" };
}

std::unique_ptr<JSONObject> verify::runCommand() {
    // JSON reply base
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Fetch params (throws CommandException on missing/invalid according to base API)
    std::unique_ptr<MiniData> dataPtr      = getDataParam("data");
    std::unique_ptr<MiniData> pubkPtr      = getDataParam("publickey");
    std::unique_ptr<MiniData> signaturePtr = getDataParam("signature");

    const MiniData& data      = *dataPtr;
    const MiniData& pubk      = *pubkPtr;
    const MiniData& signature = *signaturePtr;

    // Convert MiniData -> Signature object
    std::unique_ptr<Signature> sig = Signature::convertMiniDataVersion(signature);
    if (!sig) {
        throw CommandException("Invalid signature data");
    }

    // Extract public key from signature
    MiniData sigpubk = sig->getRootPublicKey();

    // Verify the provided public key matches the signature's root public key
    if (!pubk.isEqual(sigpubk)) {
        throw CommandException(std::string("Signature publickey is different : ") + sigpubk.to0xString());
    }

    // Prepare verifier with the public key
    TreeKey tk;
    tk.setPublicKey(sig->getRootPublicKey());

    // Verify signature against the data
    bool valid = tk.verify(data, *sig);

    if (!valid) {
        ret->put("status", false);
        ret->put("message", std::string("Signature NOT valid"));
    } else {
        ret->put("response", std::string("Signature valid"));
    }

    return ret;
}

org::minima::system::commands::Command* verify::getFunction() {
    return new verify();
}

} // namespace signatures
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org