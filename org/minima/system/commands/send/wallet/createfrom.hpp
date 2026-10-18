#pragma once

#include <memory>
#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace utils {
namespace json {
class JSONObject;
class JSONArray;
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

class createfrom : public org::minima::system::commands::Command {
public:
    createfrom();

    // Note: Base getValidParams() is not virtual in provided header, so we do not mark override.
    std::vector<std::string> getValidParams() const;

    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helper to run a single command and return the first JSONObject result
    std::shared_ptr<org::minima::utils::json::JSONObject> runCommandJSON(const std::string& zCommand);
};

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org