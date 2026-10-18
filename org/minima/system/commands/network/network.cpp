#include "org/minima/system/commands/network/network.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_traffic.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"
#include "org/minima/utils/messages/message_processor.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <string>
#include <vector>
#include <memory>
#include <type_traits>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

network::network()
    : org::minima::system::commands::Command(
          "network",
          "(action:list|reset|recalculateip) - Show network status or reset traffic counter") {
}

std::string network::getFullHelp() const {
    return std::string("\nnetwork\n"
                       "\n"
                       "Show network status or reset traffic counter.\n"
                       "\n"
                       "action: (optional)\n"
                       "    list : List the direct peers you are connected to. The default.\n"
                       "    reset : Restart the traffic counter from 0.\n"
                       "    recalculateip : Reset your IP - when you move to a different WiFi.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "network\n"
                       "\n"
                       "network action:list\n"
                       "\n"
                       "network action:reset\n");
}

std::vector<std::string> network::getValidParams() const {
    // Matches Java: only "action" is listed even though "uid" can be used optionally.
    return std::vector<std::string>{"action"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> network::runCommand() {
    using org::minima::system::Main;
    using org::minima::system::network::NetworkManager;
    using org::minima::system::network::minima::NIOClientInfo;
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    const std::string action = getParam("action", "list");

    if (action == "list") {
        const std::string uid = getParam("uid", "");

        // Get the NIO details
        NetworkManager* nm = &Main::getInstance()->getNetworkManager();
        JSONArray clarr;

        if (nm != nullptr) {
            auto& nio = nm->getNIOManager();

            // Deduce the container type and iterate robustly over possible element types.
            auto clients = nio.getAllConnectionInfo();

            for (auto& elem : clients) {
                const NIOClientInfo* info = elem.get();

                if (!info) {
                    continue;
                }

                if (uid.empty()) {
                    clarr.add(info->toJSON());
                } else if (uid == info->getUID()) {
                    clarr.add(info->toJSON());
                    break;
                }
            }
        }

        JSONObject resp;
        resp.put("connections", clarr);

        // Network details..
        NetworkManager* netmanager = &Main::getInstance()->getNetworkManager();
        if (netmanager != nullptr) {
            resp.put("details", netmanager->getStatus(true));
        }

        // Add to the response
        ret->put("response", resp);

    } else if (action == "reset") {
        // Reset traffic counter (match Java path via Main NIOManager)
        Main::getInstance()->getNIOManager().getTrafficListener().reset();
        ret->put("response", std::string("Traffic counter restarted.."));

    } else if (action == "recalculateip") {
        Main::getInstance()->getNetworkManager().calculateHostIP();

        JSONObject ip;
        ip.put("ip", org::minima::system::params::GeneralParams::MINIMA_HOST);

        ret->put("response", ip);

    } else if (action == "restart") {
        // In Java, a message is posted; in C++ we provide a direct restart trigger.
        Main::getInstance()->restartNIO();

        // Add to the response
        ret->put("response", std::string("Restarting.."));

    } else if (action == "loggingon") {
        Main::getInstance()->getNetworkManager().getNIOManager().setFullLogging(true, std::string(""));
        Main::getInstance()->getNetworkManager().getP2PManager().setFullLogging(true, std::string(""));

        // Add to the response
        ret->put("response", std::string("Full Network logging ON"));

    } else if (action == "loggingoff") {
        Main::getInstance()->getNetworkManager().getNIOManager().setFullLogging(false, std::string(""));
        Main::getInstance()->getNetworkManager().getP2PManager().setFullLogging(false, std::string(""));

        // Add to the response
        ret->put("response", std::string("Full Network logging OFF"));

    } else {
        throw org::minima::system::commands::CommandException("Invalid action");
    }

    return ret;
}

org::minima::system::commands::Command* network::getFunction() {
    return new network();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org