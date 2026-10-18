#pragma once

#include <string>

namespace org {
namespace minima {
namespace utils {
namespace json {
class JSONObject;
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace sendpoll {

class SendPollMessage {
public:
    // Constructors
    explicit SendPollMessage(const std::string& zCommand);
    SendPollMessage(const std::string& zUID, const std::string& zCommand);

    // Accessors
    std::string getUID() const;
    std::string getCommand() const;

    // Serialization
    org::minima::utils::json::JSONObject toJSON() const;

    // Copy (clone-style)
    SendPollMessage copy() const;

private:
    std::string mUID;
    std::string mCommand;
};

} // namespace sendpoll
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org