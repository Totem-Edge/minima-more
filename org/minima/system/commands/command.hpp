#pragma once

#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

// Namespaced forward declarations for project dependencies (Rule 10)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; class JSONArray; } } } }
namespace org { namespace minima { namespace objects { class Address; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {

// class CommandException : public std::runtime_error {
// public:
//     explicit CommandException(const std::string& what_arg) : std::runtime_error(what_arg) {}
// };

class Command {
public:
    // Constructors / Destructor
    Command(const std::string& zName, const std::string& zHelp);
    virtual ~Command(); // explicit declaration due to unique_ptr to forward-declared type

    // Move operations (explicit, required by Pitfall 1)
    Command(Command&&) noexcept;
    Command& operator=(Command&&) noexcept;

    // Delete copy operations
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;

    // Basic info
    void setMiniDAPPID(const std::string& zMiniDAPPID);
    std::string getMiniDAPPID() const;

    void setCompleteCommand(const std::string& zCommand);
    std::string getCompleteCommand() const;

    std::vector<std::string> getValidParams() const;

    std::string getHelp() const;
    virtual std::string getFullHelp() const;
    std::string getName() const;

    // JSON reply (Java returned JSONObject). We return unique_ptr for RAII.
    std::unique_ptr<org::minima::utils::json::JSONObject> getJSONReply() const;

    // Params access
    // Non-const to allow mutation similar to Java's getParams()
    org::minima::utils::json::JSONObject& getParams();
    const org::minima::utils::json::JSONObject& getParams() const;

    bool existsParam(const std::string& zParamName) const;

    // String params
    std::string getParam(const std::string& zParamName) const; // throws CommandException on missing/blank
    std::string getParam(const std::string& zParamName, const std::string& zDefault) const; // throws on blank if exists

    // Boolean params
    bool getBooleanParam(const std::string& zParamName) const; // "true" -> true else false
    bool getBooleanParam(const std::string& zParamName, bool zDefault) const;

    // Number params
    std::unique_ptr<org::minima::objects::base::MiniNumber> getNumberParam(const std::string& zParamName) const;
    std::unique_ptr<org::minima::objects::base::MiniNumber> getNumberParam(
        const std::string& zParamName,
        const org::minima::objects::base::MiniNumber& zDefault) const;

    // Data params
    std::unique_ptr<org::minima::objects::base::MiniData> getDataParam(const std::string& zParamName) const;
    std::unique_ptr<org::minima::objects::base::MiniData> getDataParam(
        const std::string& zParamName,
        const org::minima::objects::base::MiniData& zDefault) const;

    // JSONObject / JSONArray params
    std::unique_ptr<org::minima::utils::json::JSONObject> getJSONObjectParam(const std::string& zParamName) const;
    std::unique_ptr<org::minima::utils::json::JSONObject> getJSONObjectParam(
        const std::string& zParamName,
        const org::minima::utils::json::JSONObject& zDefault) const;

    std::unique_ptr<org::minima::utils::json::JSONArray> getJSONArrayParam(const std::string& zParamName) const;

    // Address params (returns normalized string)
    std::string getAddressParam(const std::string& zParamName) const;
    std::string getAddressParam(const std::string& zParamName, const std::string& zDefault) const;

    // Type checks
    bool isParamJSONObject(const std::string& zParamName) const;
    bool isParamJSONArray(const std::string& zParamName) const;

    // Abstract functions
    virtual std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() = 0;
    virtual Command* getFunction() = 0;

private:
    std::string mName;
    std::string mHelp;
    std::unique_ptr<org::minima::utils::json::JSONObject> mParams;
    std::string mCompleteCommand;
    std::string mMiniDAPPID;

    // Helpers
    static std::string trim(const std::string& s);
    static std::string toLower(const std::string& s);
    static bool startsWith(const std::string& s, const std::string& prefix);
};

} // namespace commands
} // namespace system
} // namespace minima
} // namespace org