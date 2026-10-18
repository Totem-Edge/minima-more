#include "org/minima/system/commands/scripts/runscript.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/string_value.hpp"

#include "org/minima/objects/address.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace scripts {

using org::minima::kissvm::Contract;
using org::minima::kissvm::values::StringValue;
using org::minima::kissvm::values::Value;
using org::minima::objects::Address;
using org::minima::objects::ScriptProof;
using org::minima::objects::StateVariable;
using org::minima::objects::Transaction;
using org::minima::objects::Witness;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::objects::mmr::MMRProof;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

// Helper: uppercase a string (ASCII)
static std::string to_upper_ascii(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// Minimal JSON string unescaper for a limited set of escapes
static std::string parse_json_string(const std::string& s, std::size_t& i) {
    // assumes s[i] == '"'
    if (i >= s.size() || s[i] != '"') {
        throw std::runtime_error("Invalid JSON: expected '\"' at string start");
    }
    ++i; // skip opening quote
    std::string out;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') {
            // end of string
            return out;
        }
        if (c == '\\') {
            if (i >= s.size()) throw std::runtime_error("Invalid JSON: trailing backslash in string");
            char esc = s[i++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                {
                    // Minimal handling: copy raw \uXXXX sequence as-is into output (preserves content as Java would read string)
                    // Read next 4 hex digits if present
                    if (i + 4 <= s.size()) {
                        out.append("\\u");
                        out.append(s.substr(i, 4));
                        i += 4;
                    } else {
                        // malformed, but store what remains
                        out.append("\\u");
                    }
                    break;
                }
                default:
                    // Unknown escape; keep as-is
                    out.push_back(esc);
                    break;
            }
        } else {
            out.push_back(c);
        }
    }
    throw std::runtime_error("Invalid JSON: unterminated string");
}

// Skip whitespace
static void skip_ws(const std::string& s, std::size_t& i) {
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            ++i;
        } else {
            break;
        }
    }
}

// Parse a flat JSON object with string keys and string values into vector of pairs
static std::vector<std::pair<std::string, std::string>>
parse_flat_string_object(const JSONObject& obj) {
    std::string json = obj.toJSONString();
    std::vector<std::pair<std::string, std::string>> result;

    std::size_t i = 0;
    skip_ws(json, i);
    if (i >= json.size() || json[i] != '{') {
        // Allow empty default "{}" or unexpected; treat as empty
        return result;
    }
    ++i; // skip '{'

    skip_ws(json, i);
    if (i < json.size() && json[i] == '}') {
        ++i;
        return result; // empty object
    }

    while (i < json.size()) {
        skip_ws(json, i);
        if (i >= json.size() || json[i] != '"') {
            // Non-string key -> not supported by this command
            throw std::runtime_error("Invalid JSON: expected string key");
        }
        std::string key = parse_json_string(json, i);

        skip_ws(json, i);
        if (i >= json.size() || json[i] != ':') {
            throw std::runtime_error("Invalid JSON: expected ':' after key");
        }
        ++i; // skip ':'

        skip_ws(json, i);
        if (i >= json.size() || json[i] != '"') {
            // Only string values supported per command usage
            throw std::runtime_error("Invalid JSON: expected string value");
        }
        std::string value = parse_json_string(json, i);

        result.emplace_back(std::move(key), std::move(value));

        skip_ws(json, i);
        if (i < json.size() && json[i] == ',') {
            ++i; // next pair
            continue;
        } else if (i < json.size() && json[i] == '}') {
            ++i; // end object
            break;
        } else {
            // Tolerate trailing whitespace before '}'
            skip_ws(json, i);
            if (i < json.size() && json[i] == '}') {
                ++i;
                break;
            }
            // Malformed
            throw std::runtime_error("Invalid JSON: expected ',' or '}' in object");
        }
    }

    return result;
}

// Convert JSONArray elements to vector of strings
static std::vector<std::string> json_array_to_strings(const JSONArray& arr) {
    std::vector<std::string> out;
    const auto& elems = arr.elements();
    out.reserve(elems.size());
    for (const auto& anyv : elems) {
        if (anyv.type() == typeid(std::string)) {
            out.emplace_back(std::any_cast<std::string>(anyv));
        } else if (anyv.type() == typeid(const char*)) {
            out.emplace_back(std::any_cast<const char*>(anyv));
        } else if (anyv.type() == typeid(char*)) {
            out.emplace_back(std::any_cast<char*>(anyv));
        } else {
            // Fallback: stringify via JSONWriter? Not available; use type name
            throw std::runtime_error("Invalid signatures array: elements must be strings");
        }
    }
    return out;
}

runscript::runscript()
    : org::minima::system::commands::Command(
          "runscript",
          "[script:] (state:{}) (prevstate:{}) (globals:{}) (signatures:[]) (extrascripts:{}) - Run a script with the defined parameters") {
}

std::string runscript::getFullHelp() const {
    return std::string("\nrunscript\n")
        + "\n"
        + "Test run a script with predefined parameters without executing on chain.\n"
        + "\n"
        + "Scripts will be auto cleaned for you.\n"
        + "\n"
        + "script:\n"
        + "    The script to run, surrounded by double quotes.\n"
        + "\n"
        + "state: (optional)\n"
        + "    State variable values to use when running the script.\n"
        + "    JSON object in the format {0:value,1:value,..}.\n"
        + "\n"
        + "prevstate: (optional)\n"
        + "    The previous state variable values (for the input coin) to use when running the script.\n"
        + "    JSON object in the format {0:value,1:value,..}.\n"
        + "\n"
        + "globals: (optional)\n"
        + "    The Global variable values to use when running the script.\n"
        + "    JSON object in the format {@GLOBAL:value,..}.\n"
        + "\n"
        + "signatures: (optional)\n"
        + "    The signatures required for the script. JSON array.\n"
        + "\n"
        + "extrascripts: (optional)\n"
        + "    Extra scripts required for MAST contracts. \n"
        + "    JSON object in the format {script:proof,..}.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "runscript script:\"RETURN SIGNEDBY(0xFF..) AND @BLOCK GT 100\" globals:{\"@BLOCK\":\"101\"} signatures:[\"0xFF\"]\n"
        + "\n"
        + "runscript script:\"LET st=STATE(99) LET ps=PREVSTATE(99) IF st EQ ps AND @COINAGE GT 20\n"
        + "AND SIGNEDBY(0xFF) THEN RETURN TRUE ELSEIF st GT ps AND SIGNEDBY(0xEE) THEN RETURN TRUE ENDIF\"\n"
        + "globals:{\"@COINAGE\":\"23\"} state:{\"99\":\"0\"} prevstate:{\"99\":\"0\"} signatures:[\"0xFF\"]\n"
        + "\n"
        + "runscript script:\"MAST 0x0E3..\" extrascripts:{\"RETURN TRUE\":\"0x000..\"}\n";
}

std::vector<std::string> runscript::getValidParams() const {
    return std::vector<std::string>{ "script", "state", "prevstate", "globals", "signatures", "extrascripts" };
}

std::unique_ptr<JSONObject> runscript::runCommand() {
    // Prepare reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get the script (default "")
    std::string script = getParam("script", "");

    // Collect optional parameters
    // state
    std::unique_ptr<JSONObject> state_ptr;
    if (existsParam("state")) {
        state_ptr = getJSONObjectParam("state");
    } else {
        state_ptr = std::make_unique<JSONObject>();
    }

    // prevstate
    std::unique_ptr<JSONObject> prevstate_ptr;
    if (existsParam("prevstate")) {
        prevstate_ptr = getJSONObjectParam("prevstate");
    } else {
        prevstate_ptr = std::make_unique<JSONObject>();
    }

    // globals
    std::unique_ptr<JSONObject> globals_ptr;
    if (existsParam("globals")) {
        globals_ptr = getJSONObjectParam("globals");
    } else {
        globals_ptr = std::make_unique<JSONObject>();
    }

    // signatures
    std::unique_ptr<JSONArray> signatures_ptr;
    if (existsParam("signatures")) {
        signatures_ptr = getJSONArrayParam("signatures");
    } else {
        signatures_ptr = std::make_unique<JSONArray>();
    }

    // extrascripts
    std::unique_ptr<JSONObject> extrascripts_ptr;
    if (existsParam("extrascripts")) {
        extrascripts_ptr = getJSONObjectParam("extrascripts");
    } else {
        extrascripts_ptr = std::make_unique<JSONObject>();
    }

    // The Transaction and Witness
    Transaction trans;
    Witness witness;

    // Add the state variables
    {
        auto entries = parse_flat_string_object(*state_ptr);
        for (const auto& kv : entries) {
            const std::string& portstr = kv.first;
            const std::string& var = kv.second;
            int port = std::stoi(portstr);
            trans.addStateVariable(std::make_unique<StateVariable>(port, var));
        }
    }

    // Add the previous state variables
    std::vector<std::unique_ptr<StateVariable>> pstate;
    {
        auto entries = parse_flat_string_object(*prevstate_ptr);
        pstate.reserve(entries.size());
        for (const auto& kv : entries) {
            const std::string& portstr = kv.first;
            const std::string& var = kv.second;
            int port = std::stoi(portstr);
            pstate.emplace_back(std::make_unique<StateVariable>(port, var));
        }
    }

    // Add the signatures
    std::vector<MiniData> sigs;
    {
        std::vector<std::string> sigstrs = json_array_to_strings(*signatures_ptr);
        sigs.reserve(sigstrs.size());
        for (const auto& s : sigstrs) {
            sigs.emplace_back(s);
        }
    }

    // Any extra scripts (MAST)
    {
        auto entries = parse_flat_string_object(*extrascripts_ptr);
        for (const auto& kv : entries) {
            const std::string& exscript = kv.first;
            const std::string& proof = kv.second;

            MiniData proofdata(proof);
            MMRProof scproof = MMRProof::convertMiniDataVersion(proofdata);

            auto scprf = std::make_unique<ScriptProof>(exscript, scproof);
            witness.addScript(std::move(scprf));
        }
    }

    // Create the Contract
    Contract contract(script, sigs, witness, trans, std::move(pstate));

    // Set the @SCRIPT global
    contract.setGlobalVariable("@SCRIPT", std::make_unique<StringValue>(script));

    // Handle @COINAGE if possible
    {
        auto globals_entries = parse_flat_string_object(*globals_ptr);
        // Convert to an unordered_map for lookup and updates
        std::unordered_map<std::string, std::string> globals_map;
        globals_map.reserve(globals_entries.size());
        for (const auto& kv : globals_entries) {
            globals_map[kv.first] = kv.second;
        }

        if (globals_map.find("@BLOCK") != globals_map.end() &&
            globals_map.find("@CREATED") != globals_map.end() &&
            globals_map.find("@COINAGE") == globals_map.end()) {
            MiniNumber block(globals_map["@BLOCK"]);
            MiniNumber inblock(globals_map["@CREATED"]);
            MiniNumber blockdiff = block.sub(inblock);
            globals_map["@COINAGE"] = blockdiff.toString();
        }

        // Set all globals (upper-cased keys)
        for (const auto& kv : globals_map) {
            std::string upper = to_upper_ascii(kv.first);
            contract.setGlobalVariable(upper, Value::getValue(kv.second));
        }
    }

    // Run it
    contract.run();

    bool parseok   = contract.isParseOK();
    bool monotonic = contract.isMonotonic();
    bool success   = contract.isSuccess();

    // Build response
    JSONObject resp;

    // Normal script info
    {
        JSONObject scriptnormal;
        scriptnormal.put("script", script);

        MiniData addr = Address(script).getAddressData();
        scriptnormal.put("address", addr.to0xString());
        scriptnormal.put("mxaddress", Address::makeMinimaAddress(addr));

        resp.put("script", scriptnormal);
    }

    // Cleaned script info
    {
        JSONObject scriptclean;
        std::string cleanscript = Contract::cleanScript(script);
        scriptclean.put("script", cleanscript);

        MiniData addr = Address(cleanscript).getAddressData();
        scriptclean.put("address", addr.to0xString());
        scriptclean.put("mxaddress", Address::makeMinimaAddress(addr));

        resp.put("clean", scriptclean);
    }

    // Trace
    resp.put("trace", contract.getCompleteTraceLog());

    // Variables
    resp.put("variables", contract.getAllVariables());

    // Flags
    resp.put("parseok", parseok);
    resp.put("monotonic", monotonic);
    resp.put("success", success);

    // Put response
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* runscript::getFunction() {
    return new runscript();
}

} // namespace scripts
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org