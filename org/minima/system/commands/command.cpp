#include "org/minima/system/commands/command.hpp"

#include <algorithm>
#include <cctype>
#include <any>
#include <typeinfo>
#include <memory>
#include <string>

// Full headers for used classes except Address
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
// FIX: Include the definition of CommandException needed for throwing
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/objects/address.hpp"


#ifdef _WIN32
// No OS-specific code required currently.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

Command::Command(const std::string& zName, const std::string& zHelp)
    : mName(zName)
    , mHelp(zHelp)
    , mParams(std::make_unique<JSONObject>())
    , mCompleteCommand("")
    , mMiniDAPPID("") {}

Command::~Command() = default;
Command::Command(Command&&) noexcept = default;
Command& Command::operator=(Command&&) noexcept = default;

void Command::setMiniDAPPID(const std::string& zMiniDAPPID) {
    mMiniDAPPID = zMiniDAPPID;
}

std::string Command::getMiniDAPPID() const {
    return mMiniDAPPID;
}

void Command::setCompleteCommand(const std::string& zCommand) {
    mCompleteCommand = zCommand;
}

std::vector<std::string> Command::getValidParams() const {
    return {};
}

std::string Command::getCompleteCommand() const {
    return mCompleteCommand;
}

std::string Command::getHelp() const {
    return mHelp;
}

std::string Command::getFullHelp() const {
    return mHelp; // Assumes simple help is full help if not overridden
}

std::unique_ptr<JSONObject> Command::getJSONReply() const {
    auto json = std::make_unique<JSONObject>();
    json->put("command", mName);

    if (mParams && mParams->size() > 0) { // Check if mParams is valid and has elements
        json->put("params", *mParams); // Put a copy
    }

    json->put("status", true);
    json->put("pending", false);
    return json;
}

std::string Command::getName() const {
    return mName;
}

JSONObject& Command::getParams() {
    if (!mParams) { // Should not happen with current constructor, but safe check
        mParams = std::make_unique<JSONObject>();
    }
    return *mParams;
}

const JSONObject& Command::getParams() const {
     if (!mParams) {
         // Cannot create in const method, throw or return a static empty one
         throw std::runtime_error("Command parameters accessed when null (const)");
         // static const JSONObject empty_params; return empty_params; // Alternative
    }
    return *mParams;
}

bool Command::existsParam(const std::string& zParamName) const {
    return mParams && mParams->containsKey(zParamName);
}

std::string Command::getParam(const std::string& zParamName) const {
    if (!existsParam(zParamName)) {
        // Now CommandException is defined
        throw CommandException("param not specified : " + zParamName);
    }

    const std::any& aval = mParams->get(zParamName);
    std::string pp;

    try {
        pp = std::any_cast<std::string>(aval);
    } catch (const std::bad_any_cast&) {
        // Throw CommandException if it's not a string
        throw CommandException("param is not a string : " + zParamName);
    }

    pp = trim(pp);
    if (pp.empty()) {
        // Now CommandException is defined
        throw CommandException("BLANK param not allowed : " + zParamName);
    }

    return pp;
}

std::string Command::getParam(const std::string& zParamName, const std::string& zDefault) const {
    if (existsParam(zParamName)) {
        const std::any& aval = mParams->get(zParamName);
        std::string pp;

        try {
            pp = std::any_cast<std::string>(aval);
        } catch (const std::bad_any_cast&) {
            // Throw CommandException if it exists but is not a string
             throw CommandException("param is not a string : " + zParamName);
        }

        pp = trim(pp);
        if (pp.empty()) {
            // Now CommandException is defined
            throw CommandException("BLANK param not allowed : " + zParamName);
        }
        return pp;
    }
    return zDefault;
}


bool Command::getBooleanParam(const std::string& zParamName) const {
    std::string bools = getParam(zParamName); // throws if missing/blank/not string
    std::string lower_bools = toLower(bools);
    return (lower_bools == "true");
}

bool Command::getBooleanParam(const std::string& zParamName, bool zDefault) const {
    if (existsParam(zParamName)) {
        // Use getParam which handles blank/type errors
        std::string bools = getParam(zParamName);
        std::string lower_bools = toLower(bools);
        return (lower_bools == "true");
    }
    return zDefault;
}

std::unique_ptr<MiniNumber> Command::getNumberParam(const std::string& zParamName) const {
    std::string num_str = getParam(zParamName); // throws if missing/blank/not string
    try {
        return std::make_unique<MiniNumber>(num_str);
    } catch (const std::exception& e) { // Catch potential MiniNumber construction errors
         throw CommandException("Invalid number format for param '" + zParamName + "': " + num_str + " (" + e.what() + ")");
    }
}

std::unique_ptr<MiniNumber> Command::getNumberParam(
    const std::string& zParamName,
    const MiniNumber& zDefault) const
{
    if (existsParam(zParamName)) {
        return getNumberParam(zParamName); // Handles errors internally
    }
    return std::make_unique<MiniNumber>(zDefault); // Copy default
}


std::unique_ptr<MiniData> Command::getDataParam(const std::string& zParamName) const {
    std::string hex = getParam(zParamName);
    try {
        std::string lower = toLower(hex);
        if (startsWith(lower, "mx")) {
            // The function returns a MiniData object, not a pointer.
            // We must construct a new unique_ptr from that object.
            return std::make_unique<MiniData>(org::minima::objects::Address::convertMinimaAddress(hex));
        } else {
             return std::make_unique<MiniData>(hex);
        }
    } catch (const std::exception& e) {
        // Now CommandException is defined
        throw CommandException("Invalid Data param specified for '" + zParamName + "': " + hex + " (" + e.what() + ")");
    }
    // The warning should disappear as all paths either return or throw
}

std::unique_ptr<MiniData> Command::getDataParam(
    const std::string& zParamName,
    const MiniData& zDefault) const
{
    if (existsParam(zParamName)) {
        return getDataParam(zParamName); // Handles errors internally
    }
    return std::make_unique<MiniData>(zDefault); // Copy default
}

std::unique_ptr<JSONObject> Command::getJSONObjectParam(const std::string& zParamName) const {
    if (!existsParam(zParamName)) {
         // Now CommandException is defined
        throw CommandException("param not specified : " + zParamName);
    }

    const std::any& aval = mParams->get(zParamName);

    // Check if it holds a JSONObject directly
    if (aval.type() == typeid(JSONObject)) {
        // Need to create a copy for the unique_ptr
        return std::make_unique<JSONObject>(std::any_cast<const JSONObject&>(aval));
    }
    // Check if it holds a pointer (less common, but possible)
    else if (aval.type() == typeid(JSONObject*)) {
        const JSONObject* obj_ptr = std::any_cast<JSONObject*>(aval); // Use const cast
        if (obj_ptr) {
            return std::make_unique<JSONObject>(*obj_ptr); // Copy construct
        } else {
             throw CommandException("param '" + zParamName + "' holds a null JSONObject pointer");
        }
    }

    // Type mismatch
     throw CommandException("param '" + zParamName + "' is not a JSONObject");
     // The warning should disappear
}

std::unique_ptr<JSONObject> Command::getJSONObjectParam(
    const std::string& zParamName,
    const JSONObject& zDefault) const
{
    if (!existsParam(zParamName)) {
        return std::make_unique<JSONObject>(zDefault); // Copy default
    }
    return getJSONObjectParam(zParamName); // Handles errors internally
}

std::unique_ptr<JSONArray> Command::getJSONArrayParam(const std::string& zParamName) const {
    if (!existsParam(zParamName)) {
         // Now CommandException is defined
        throw CommandException("param not specified : " + zParamName);
    }

    const std::any& aval = mParams->get(zParamName);

    if (aval.type() == typeid(JSONArray)) {
        return std::make_unique<JSONArray>(std::any_cast<const JSONArray&>(aval)); // Copy
    } else if (aval.type() == typeid(JSONArray*)) {
        const JSONArray* arr_ptr = std::any_cast<JSONArray*>(aval); // Use const cast
        if (arr_ptr) {
            return std::make_unique<JSONArray>(*arr_ptr); // Copy
        } else {
             throw CommandException("param '" + zParamName + "' holds a null JSONArray pointer");
        }
    }

    // Type mismatch
     throw CommandException("param '" + zParamName + "' is not a JSONArray");
     // The warning should disappear
}


std::string Command::getAddressParam(const std::string& zParamName, const std::string& zDefault) const {
    if (existsParam(zParamName)) {
        return getAddressParam(zParamName); // Handles errors internally
    }
    return zDefault;
}

std::string Command::getAddressParam(const std::string& zParamName) const {
    if (!existsParam(zParamName)) {
        // Now CommandException is defined
        throw CommandException("param not specified : " + zParamName);
    }

    std::string address = getParam(zParamName); // throws if missing/blank/not string
    std::string lower = toLower(address);

    if (startsWith(lower, "mx")) {
        try {
            // Call the function and get the MiniData object by value
            MiniData md = org::minima::objects::Address::convertMinimaAddress(address);
            
            // Call to0xString() on the object itself
            address = md.to0xString(); 
        } catch (const std::exception& exc) {
            throw CommandException(std::string("Error converting Minima Address '") + address + "': " + exc.what());
        }
    }
    // Check if it looks like a hex address AFTER potential conversion
    if (startsWith(address, "0x")) {
        try {
            MiniData data(address); // Validate hex format
            address = data.to0xString(); // Normalize (e.g., case)
        } catch (const std::exception& exc) {
             // Now CommandException is defined
            throw CommandException(std::string("Invalid 0x address format for param '") + zParamName + "': " + address + " (" + exc.what() + ")");
        }
        // Catch block removed
    }
    // If it's not Mx and not 0x, consider it invalid? Or allow raw scripts?
    // Current logic passes it through if it's not Mx or 0x.
    // else {
    //    throw CommandException("Address param '" + zParamName + "' must start with Mx or 0x: " + address);
    // }


    return address;
}

bool Command::isParamJSONObject(const std::string& zParamName) const {
    if (existsParam(zParamName)) {
        const std::any& obj = mParams->get(zParamName);
        return (obj.type() == typeid(JSONObject) || obj.type() == typeid(JSONObject*));
    }
    return false;
}

bool Command::isParamJSONArray(const std::string& zParamName) const {
    if (existsParam(zParamName)) {
        const std::any& obj = mParams->get(zParamName);
        return (obj.type() == typeid(JSONArray) || obj.type() == typeid(JSONArray*));
    }
    return false;
}

// Helpers (static methods)
std::string Command::trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return ""; // String is all whitespace
    auto end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start, end - start + 1);
}

std::string Command::toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return out;
}

bool Command::startsWith(const std::string& s, const std::string& prefix) {
    return s.find(prefix) == 0;
}

} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
