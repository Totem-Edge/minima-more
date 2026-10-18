#include "org/minima/system/commands/base/checkaddress.hpp"

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <string>

#ifdef _WIN32
// No OS-specific behavior required for this command currently.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

checkaddress::checkaddress()
    : org::minima::system::commands::Command(
          "checkaddress",
          "[address:] - Check an address is valid") {}

std::vector<std::string> checkaddress::getValidParams() const {
    return std::vector<std::string>{ "address" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> checkaddress::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::wallet::Wallet;
    using org::minima::objects::Address;
    using org::minima::objects::base::MiniData;
    using org::minima::utils::json::JSONObject;
    using org::minima::system::commands::CommandException;

    std::unique_ptr<JSONObject> ret = getJSONReply();

    // First get the address
    std::string address = getAddressParam("address");

    if (address.rfind("0x", 0) == 0 && address.length() != 66) {
        // Hmm. should be 66 chars long..
        throw CommandException(
            std::string("Invalid Length for 0x address should be 66 chars long : ") +
            std::to_string(address.length()));
    }

    if (!(address.rfind("Mx", 0) == 0) && !(address.rfind("0x", 0) == 0)) {
        throw CommandException("Address does not start with 0x or Mx");
    }

    // Check if this is one of our addresses
    Wallet& wallet = MinimaDB::getDB()->getWallet();

    MiniData data(address);
    std::string datastr = data.to0xString();

    JSONObject res;
    res.put("original", address);
    res.put("0x", datastr);
    res.put("Mx", Address::makeMinimaAddress(data));
    res.put("relevant", wallet.isAddressRelevant(datastr));
    res.put("simple", wallet.isAddressSimple(datastr));

    ret->put("response", res);

    return ret;
}

org::minima::system::commands::Command* checkaddress::getFunction() {
    return new checkaddress();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org