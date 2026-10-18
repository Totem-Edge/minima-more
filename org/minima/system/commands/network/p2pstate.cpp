#include "org/minima/system/commands/network/p2pstate.hpp"

#include <stdexcept>

#include "org/minima/system/main.hpp"
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message_processor.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

p2pstate::p2pstate()
    : org::minima::system::commands::Command("p2pstate", "prints full details of the internal p2p state") {
}

std::string p2pstate::getFullHelp() const {
    return "\np2pstate\n"
           "\n"
           "Prints full details of the internal p2p state.\n"
           "\n"
           "Includes details of your in and out connections and total peers.\n"
           "\n"
           "Examples:\n"
           "\n"
           "p2pstate\n";
}

std::unique_ptr<org::minima::utils::json::JSONObject> p2pstate::runCommand() {
    auto ret = getJSONReply();

    org::minima::system::Main* main = org::minima::system::Main::getInstance();
    auto* netman = &main->getNetworkManager();

    // getP2PManager() returns a MessageProcessor (likely by reference). Bind to a reference.
    org::minima::utils::messages::MessageProcessor& base = netman->getP2PManager();

    // Mirror Java's explicit cast: (P2PManager) ...
    auto* p2pman = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(&base);
    if (!p2pman) {
        throw std::runtime_error("NetworkManager::getP2PManager is not a P2PManager");
    }

    // Store the full status JSON into the reply
    ret->put("p2p-state", p2pman->getStatus(true));

    return ret;
}

org::minima::system::commands::Command* p2pstate::getFunction() {
    return new p2pstate();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org