#include "org/minima/system/commands/network/message.hpp"

#include <stdexcept>

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

message::message()
    : org::minima::system::commands::Command(
          "message",
          "[data:message] (uid:uid) - Send a message over the network to one of your direct peers") {
}

std::string message::getFullHelp() const {
    return
        "\nmessage\n"
        "\n"
        "Send a message to one or all of your direct peers.\n"
        "\n"
        "data:\n"
        "    The message as a string.\n"
        "\n"
        "uid: (optional)\n"
        "    Leave blank to send a message to all peers or enter the uid of the peer to send the message to.\n"
        "    uid can be found from the 'network' command.\n"
        "\n"
        "Examples:\n"
        "\n"
        "message data:\"hello\" uid:CVNPMLPOCQ0HQ\n";
}

std::vector<std::string> message::getValidParams() const {
    return {"uid", "data"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> message::runCommand() {
    auto ret = getJSONReply();

    // Access params
    org::minima::utils::json::JSONObject& params = getParams();

    // Get the data
    if (!params.containsKey("data")) {
        throw std::runtime_error("No data specified");
    }
    std::string data = params.getString("data");

    // Is there a UID
    std::string uid;
    if (!params.containsKey("uid")) {
        uid = "";
        ret->put("message", std::string("Message sent to all"));
    } else {
        uid = params.getString("uid");
        ret->put("message", std::string("Message sent to ") + uid);
    }

    // Create a message..
    org::minima::objects::base::MiniString msg(data);

    // Message type as MiniByte (convert from NIOMessage integer code)
    org::minima::objects::base::MiniByte type(
        static_cast<unsigned char>(org::minima::system::network::minima::NIOMessage::MSG_GENMESSAGE().getValue()));

    // Send it..
    org::minima::system::Main::getInstance()
        ->getNIOManager()
        .sendNetworkMessage(uid, type, msg);

    return ret;
}

org::minima::system::commands::Command* message::getFunction() {
    return new message();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org