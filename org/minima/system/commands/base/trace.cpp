#include "org/minima/system/commands/base/trace.hpp"

#include <stdexcept>

#include "org/minima/system/network/minima/n_i_o_client.hpp"
#include "org/minima/system/network/minima/n_i_o_server.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message_processor.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

trace::trace()
    : org::minima::system::commands::Command(
          "trace",
          "[enable:true|false] (filter:) (network:) - Show the message stacks of the internal Minima Engine with optional filter string. Only works on terminal.") {}

std::string trace::getFullHelp() const {
    return
R"(
trace

Show the message stacks of the internal Minima Engine with optional filter string.

enable:
    true or false, true to enable or false to disable.

filter: (optional)
    A case sensitive string to filter the messages by.

network: (optional)
    true or false, show low level network messages

Examples:

trace enable:true

trace enable:true filter:MAIN

trace enable:true filter:MINER

trace enable:true filter:NOTIFYMANAGER

trace enable:true filter:TXPOWPROCESSOR

trace enable:false
)";
}

std::vector<std::string> trace::getValidParams() const {
    return { "enable", "filter", "network" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> trace::runCommand() {
    using org::minima::utils::json::JSONObject;

    // Base reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Must specify enable
    if (!existsParam("enable")) {
        throw std::runtime_error("Must specify enable");
    }

    // Read "enable"
    const std::string act = getParams().getString("enable");
    bool on = (act == "true");

    // Optional filter
    std::string filter;
    if (existsParam("filter")) {
        filter = getParams().getString("filter");
    } else {
        filter = "";
    }

    // Set trace on MessageProcessor
    org::minima::utils::messages::MessageProcessor::setTrace(on, filter);

    // Optional network flag
    bool network = getBooleanParam("network", false);

    // Apply to networking trace flags
    org::minima::system::network::minima::NIOClient::mTraceON = network;
    org::minima::system::network::minima::NIOServer::mTraceON.store(network);

    // Build response
    JSONObject tr;
    tr.put("enabled", on);
    tr.put("filter", filter);
    tr.put("shownetwork", network);

    ret->put("response", tr);
    return ret;
}

org::minima::system::commands::Command* trace::getFunction() {
    return new trace();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org