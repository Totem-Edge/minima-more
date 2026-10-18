#pragma once

#include "org/minima/system/commands/command.hpp"

#include <memory>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace utils {
namespace json {
class JSONObject;
}
} // namespace utils
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace utils {
namespace messages {
class Message;
}
} // namespace utils
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

class peers final : public org::minima::system::commands::Command {
public:
    peers();

    // Java had @Override, here just regular methods
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Core command execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Static utility mirroring Java
    static std::string getPeersList(int zMaxPeers);

    // Factory
    org::minima::system::commands::Command* getFunction() override;

private:
    // Replacement for Java's connect.createConnectMessage(peer)
    static std::unique_ptr<org::minima::utils::messages::Message>
    createConnectMessage(const std::string& zPeer);

    // Helpers
    static std::string trim(const std::string& s);
    static bool startsWith(const std::string& s, const std::string& prefix);
    static std::vector<std::string> splitCSV(const std::string& s);
    static std::string joinCSV(const std::vector<std::string>& v, std::size_t maxcount);
};

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org