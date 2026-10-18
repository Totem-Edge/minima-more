#include "org/minima/system/commands/txn/txnminepost.hpp"

#include <utility>
#include <memory>

#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txnminepost::txnminepost()
    : org::minima::system::commands::Command("txnminepost", "[data:] - Post a pre-mined transaction") {
}

std::vector<std::string> txnminepost::getValidParams() const {
    return std::vector<std::string>{ "data" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnminepost::runCommand() {
    using org::minima::objects::TxPoW;
    using org::minima::objects::base::MiniData;
    using org::minima::utils::json::JSONObject;
    using org::minima::utils::messages::Message;

    // Base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get the HEX data parameter
    std::unique_ptr<MiniData> txdata = getDataParam("data");

    // Convert to a TxPoW
    std::unique_ptr<TxPoW> txp_uptr = TxPoW::convertMiniDataVersion(*txdata);

    // Ensure the TxPoW object outlives this function via shared_ptr when posting in a Message
    std::shared_ptr<TxPoW> txp_sp(std::move(txp_uptr));

    // Post the message: MAIN_TXPOWMINED with "txpow"
    auto msg = std::make_shared<Message>(org::minima::system::Main::MAIN_TXPOWMINED);
    msg->addObject("txpow", txp_sp);
    org::minima::system::Main::getInstance()->PostMessage(msg);

    // Build response JSON with the MINED txn data
    JSONObject resp;
    resp.put("data", txp_sp->toJSON());
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* txnminepost::getFunction() {
    return new txnminepost();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org