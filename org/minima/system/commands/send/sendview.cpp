#include "org/minima/system/commands/send/sendview.hpp"

#include <stdexcept>

#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

sendview::sendview()
    : org::minima::system::commands::Command(
          "sendview",
          "[file:] - View a transaction ( signed or unsigned )") {
}

std::vector<std::string> sendview::getValidParams() const {
    return std::vector<std::string>{ "file" };
}

std::string sendview::getFullHelp() const {
    return std::string()
        + "\nsendview\n"
        + "\n"
        + "View a transaction ( signed or unsigned ).\n"
        + "\n"
        + "View the details of a txn created by the 'sendnosign' command by specifying its .txn file.\n"
        + "\n"
        + "file:\n"
        + "    Name of the transaction (.txn) file to view, located in the node's base folder.\n"
        + "    If not in the base folder, specify the full file path.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "sendview file:unsignedtransaction-1674907380057.txn\n"
        + "\n"
        + "sendview file:C:\\Users\\signedtransaction-1674907380057.txn\n"
        + "\n";
}

std::unique_ptr<org::minima::utils::json::JSONObject> sendview::runCommand() {
    using org::minima::objects::TxPoW;
    using org::minima::objects::base::MiniData;
    using org::minima::utils::MiniFile;
    using org::minima::utils::json::JSONObject;

    // Base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Required parameter
    std::string txnfile = getParam("file");

    // Load the txn file bytes
    auto path = MiniFile::createBaseFile(txnfile);
    std::vector<std::uint8_t> data = MiniFile::readCompleteFile(path);

    // Create MiniData from bytes
    MiniData txndata(data);

    // Convert back into a TxPoW
    std::unique_ptr<TxPoW> txp = TxPoW::convertMiniDataVersion(txndata);

    // Build the response JSON
    JSONObject sigtran;
    sigtran.put("txpow", txp->toJSON());

    // Match Java structure
    JSONObject resp; // Unused but present in original Java before placing into ret
    ret->put("response", sigtran);

    return ret;
}

org::minima::system::commands::Command* sendview::getFunction() {
    return new sendview();
}

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org