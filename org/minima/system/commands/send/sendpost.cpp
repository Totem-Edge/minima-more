#include "org/minima/system/commands/send/sendpost.hpp"

#include <utility>
#include <stdexcept>

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

sendpost::sendpost()
    : org::minima::system::commands::Command("sendpost", "[file:] - Post a signed txn") {}

std::vector<std::string> sendpost::getValidParams() const {
    return std::vector<std::string>{ "file" };
}

std::string sendpost::getFullHelp() const {
    return
        "\nsendpost\n"
        "\n"
        "Post a transaction previously created and signed using the 'sendnosign' and 'sendsign' commands.\n"
        "\n"
        "Must be posted from an online node within approximately 24 hours of creating to ensure MMR proofs are valid.\n"
        "\n"
        "file:\n"
        "    Name of the signed transaction (.txn) file to post, located in the node's base folder.\n"
        "    If not in the base folder, specify the full file path.\n"
        "\n"
        "Examples:\n"
        "\n"
        "sendpost file:signedtransaction-1674907380057.txn\n"
        "\n"
        "sendpost file:C:\\Users\\signedtransaction-1674907380057.txn\n"
        "\n";
}

std::unique_ptr<org::minima::utils::json::JSONObject> sendpost::runCommand() {
    // Base reply object
    auto ret = getJSONReply();

    // Required parameter
    const std::string txnfile = getParam("file");

    // Load the txn bytes
    const auto filepath = org::minima::utils::MiniFile::createBaseFile(txnfile);
    const std::vector<std::uint8_t> data = org::minima::utils::MiniFile::readCompleteFile(filepath);

    // Create MiniData from raw bytes
    org::minima::objects::base::MiniData txndata(data);

    // Convert back into a TxPoW
    std::unique_ptr<org::minima::objects::TxPoW> txp =
        org::minima::objects::TxPoW::convertMiniDataVersion(txndata);
    if (!txp) {
        throw std::runtime_error("Failed to reconstruct TxPoW from MiniData.");
    }

    // Calculate the TxPOWID
    txp->calculateTXPOWID();

    // Build response JSON
    org::minima::utils::json::JSONObject sigtran;
    sigtran.put("txpow", txp->toJSON());

    ret->put("response", sigtran);

    // Post It..! (asynchronous mining)
    org::minima::system::Main* main = org::minima::system::Main::getInstance();
    if (!main) {
        throw std::runtime_error("Main instance is null.");
    }
    auto* miner = &main->getTxPoWMiner();
    if (!miner) {
        throw std::runtime_error("TxPoWMiner is null.");
    }

    // Pass by reference; miner is expected to clone internally for async processing.
    miner->mineTxPoWAsync(*txp);

    return ret;
}

org::minima::system::commands::Command* sendpost::getFunction() {
    return new sendpost();
}

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org