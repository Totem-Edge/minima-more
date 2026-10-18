#pragma once
#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/commands/command.hpp"
#include "org/minima/system/commands/command_exception.hpp"

// Forward declarations for used types (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

class sendfrom : public org::minima::system::commands::Command {
public:
    sendfrom();
    ~sendfrom() override = default;

    // Java exposes getValidParams(); not virtual in base, so we provide a matching method.
    std::vector<std::string> getValidParams() const;

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    org::minima::system::commands::Command* getFunction() override;

private:
    // Helper to run a single sub-command and return its JSONObject result
    std::shared_ptr<org::minima::utils::json::JSONObject> runCommand(const std::string& zCommand);

    // Utility: convert bool to "true"/"false"
    static std::string boolToString(bool b);

    // Utility: parse a flat JSON object string into key/value pairs
    // Recognizes: {"k":"v","n":123,"b":true} etc. Returns raw value strings as they appear logically.
    static std::vector<std::pair<std::string, std::string>> parseFlatJSONObjectString(const std::string& json);

    // Utility: unescape JSON string value (minimal, supports \" and \\)
    static std::string unescapeJSONString(const std::string& in);
};

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org