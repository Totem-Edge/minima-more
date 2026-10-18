#include "org/minima/system/commands/network/connect.hpp"

#include <stdexcept>

// Project includes
#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

connect::connect()
    : org::minima::system::commands::Command(
          "connect",
          "[host:ip:port] - Connect to a network Minima instance") {}

std::string connect::getFullHelp() const {
    return std::string("\nconnect\n")
        + "\n"
        + "Connect to a network Minima instance.\n"
        + "\n"
        + "Connect to another node to join the main network or to create a private test network.\n"
        + "\n"
        + "Set your own host using the -host parameter at start up.\n"
        + "\n"
        + "host:\n"
        + "    The external ip:port of the node to connect to.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "connect host:94.0.239.117:9001\n";
}

std::vector<std::string> connect::getValidParams() const {
    return std::vector<std::string>{"host"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> connect::runCommand() {
    // Equivalent to Java's: JSONObject ret = getJSONReply();
    auto ret = getJSONReply();

    // Get the host:port (Java used getParams().get("host") and checked for null)
    org::minima::utils::json::JSONObject& params = getParams();

    if (!params.containsKey("host")) {
        throw std::runtime_error("No host specified");
    }
    std::string fullhost = params.getString("host");

    // Create the Message
    auto msg = createConnectMessage(fullhost);
    if (!msg) {
        throw std::runtime_error("Must specify host:port");
    }

    // Post the message (expects shared_ptr<Message>)
    org::minima::system::Main::getInstance()->getNIOManager().PostMessage(msg);

    // Response
    ret->put("message", std::string("Attempting to connect to ") + fullhost);

    return ret;
}

std::shared_ptr<org::minima::utils::messages::Message>
connect::createConnectMessage(const std::string& zFullHost) {
    // Find colon separator
    std::size_t index = zFullHost.find(':');
    if (index == std::string::npos) {
        return nullptr;
    }

    std::string ip = zFullHost.substr(0, index);
    std::string ports = zFullHost.substr(index + 1);

    // Trim spaces (replicate Java .trim())
    auto trim = [](const std::string& s) -> std::string {
        const char* ws = " \t\n\r\f\v";
        std::size_t start = s.find_first_not_of(ws);
        if (start == std::string::npos) return std::string();
        std::size_t end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    };

    ip = trim(ip);
    ports = trim(ports);

    int port = 0;
    try {
        port = std::stoi(ports);
    } catch (const std::invalid_argument&) {
        return nullptr;
    } catch (const std::out_of_range&) {
        return nullptr;
    }

    // Construct message
    auto msg = std::make_shared<org::minima::utils::messages::Message>(
        org::minima::system::network::minima::NIOManager::NIO_CONNECT);
    msg->addString("host", ip);
    msg->addInteger("port", port);

    return msg;
}

org::minima::system::commands::Command* connect::getFunction() {
    return new connect();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org