#pragma once

#include <memory>
#include <string>
#include <vector>
#include <any>

#include "org/minima/system/commands/command.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

class rawfrom : public org::minima::system::commands::Command {
public:
    rawfrom();

    // Parameter specification and help
    std::vector<std::string> getValidParams() const;
    std::string getFullHelp() const override;
    // Execute command
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Factory clone
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helper to run a single command and get the resulting JSONObject
    std::shared_ptr<org::minima::utils::json::JSONObject> runCommandSingle(const std::string& zCommand);

    // Helper to obtain a JSONObject from a JSONArray element (std::any)
    static std::shared_ptr<org::minima::utils::json::JSONObject> anyToJSONObject(const std::any& a);

    // Parse a flat JSON object (string keys -> arbitrary values) into vector of (port, valueString)
    static std::vector<std::pair<int, std::string>> parseFlatStateObject(const org::minima::utils::json::JSONObject& obj);

    // Internal JSON string parsing helpers
    static void skipWS(const std::string& s, std::size_t& i);
    static std::string parseJSONStringQuoted(const std::string& s, std::size_t& i); // expects starting at '"'
    static std::string parseJSONValueToken(const std::string& s, std::size_t& i);   // non-quoted primitive token
};

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org