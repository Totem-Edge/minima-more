#include "org/minima/system/commands/sendpoll/send_poll_message.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace sendpoll {

using org::minima::objects::base::MiniData;
using org::minima::utils::json::JSONObject;

SendPollMessage::SendPollMessage(const std::string& zCommand)
    : mUID(MiniData::getRandomData(16).to0xString()),
      mCommand(zCommand) {}

SendPollMessage::SendPollMessage(const std::string& zUID, const std::string& zCommand)
    : mUID(zUID),
      mCommand(zCommand) {}

std::string SendPollMessage::getUID() const {
    return mUID;
}

std::string SendPollMessage::getCommand() const {
    return mCommand;
}

JSONObject SendPollMessage::toJSON() const {
    JSONObject ret;
    ret.put("uid", getUID());
    ret.put("command", getCommand());
    return ret;
}

SendPollMessage SendPollMessage::copy() const {
    return SendPollMessage(mUID, mCommand);
}

} // namespace sendpoll
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org