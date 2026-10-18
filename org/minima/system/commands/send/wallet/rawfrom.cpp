#include "org/minima/system/commands/send/wallet/rawfrom.hpp"

#include <stdexcept>
#include <sstream>
#include <cctype>

#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;
using org::minima::objects::StateVariable;
using org::minima::objects::base::MiniData;

rawfrom::rawfrom()
    : org::minima::system::commands::Command(
          "rawfrom",
          "[inputs:] [outputs:] (state:) - Create an unsigned transaction from a set of inputs, outputs and state") {
}

std::vector<std::string> rawfrom::getValidParams() const {
    return {"inputs","outputs","state"};
}

std::string rawfrom::getFullHelp() const {
    return std::string("\nrawfrom\n")
        + "\n"
        + "Create an unsigned transaction with a list of input and output JSON coins.\n"
        + "\n"
        + "inputs:\n"
        + "    A JSONArray of input JSON coins with a coinid and script value.\n"
        + "\n"
        + "outputs:\n"
        + "    A JSONArray of output JSON coins with an address, amount and optional tokenid and storestate.\n"
        + "\n"
        + "state: (optional)\n"
        + "    The JSON state.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "rawfrom inputs:[{\"coinid\":\"0x859EC55D1DC0DD66E72C1D8B6CDBD5E12D0EC5CB56255F629BAEC590047C22C4\",\"script\":\"RETURN SIGNEDBY(0x677228489AD4C14AC0D04F9BA054974A2B1D60C5A8E8D710CF9F2BF64D1EE81E)\"}] outputs:[{\"address\":\"0x6A3A060CE9D8E876E9B12DBE5D8F6A6864183BA1A63EA8CBCC14D668B5BECACF\",\"amount\":\"9.9996\",\"storestate\":false,\"tokenid\":\"0x00\"}] state:{\"0\":\"98\",\"1\":\"[MESSAGE]\"}\n";
}

std::unique_ptr<JSONObject> rawfrom::runCommand() {
    // Prepare response
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get inputs and outputs arrays
    std::unique_ptr<JSONArray> inputs = getJSONArrayParam("inputs");
    std::unique_ptr<JSONArray> outputs = getJSONArrayParam("outputs");

    // Optional state
    std::unique_ptr<JSONObject> state;
    if (existsParam("state")) {
        state = getJSONObjectParam("state");
    }

    // Random transaction ID
    std::string randomid = MiniData::getRandomData(32).to0xString();

    // Create transaction
    std::shared_ptr<JSONObject> result = runCommandSingle(std::string("txncreate id:") + randomid);

    // Add Inputs
    const auto& inelems = inputs->elements();
    for (const std::any& ainput : inelems) {
        std::shared_ptr<JSONObject> in = anyToJSONObject(ainput);

        std::string coinid = in->getString("coinid");
        std::string script = in->getString("script");

        // Add input coin
        runCommandSingle(std::string("txninput id:") + randomid + " coinid:" + coinid);

        // Add script
        runCommandSingle(std::string("txnscript id:") + randomid + " scripts:{\"" + script + "\":\"\"}");
    }

    // Add Outputs
    const auto& outelems = outputs->elements();
    for (const std::any& aoutput : outelems) {
        std::shared_ptr<JSONObject> out = anyToJSONObject(aoutput);

        std::string address = out->getString("address");
        std::string amount  = out->getString("amount");

        std::string tokenid = "0x00";
        if (out->containsKey("tokenid")) {
            tokenid = out->getString("tokenid");
        }

        bool storestate = true;
        if (out->containsKey("storestate")) {
            storestate = out->getBoolean("storestate");
        }

        // Add output coin
        std::ostringstream oss;
        oss << "txnoutput id:" << randomid
            << " address:" << address
            << " amount:" << amount
            << " tokenid:" << tokenid
            << " storestate:" << (storestate ? "true" : "false");
        runCommandSingle(oss.str());
    }

    // Add state if exists
    if (state) {
        // Extract port -> value pairs
        auto pairs = parseFlatStateObject(*state);
        for (const auto& kv : pairs) {
            int port = kv.first;
            const std::string& var = kv.second;

            // Create a state variable for normalization
            StateVariable sv(port, var);

            // Use sv.getPort() and sv.getData() textual form
            std::ostringstream sstate;
            sstate << "txnstate id:" << randomid
                   << " port:" << sv.getPort()
                   << " value:\""
                   << sv.getData().toString()
                   << "\"";
            runCommandSingle(sstate.str());
        }
    }

    // Export
    result = runCommandSingle(std::string("txnexport id:") + randomid + " showtxn:true");

    // Delete
    runCommandSingle(std::string("txndelete id:") + randomid);

    // Return response
    ret->put("response", result->get("response"));
    return ret;
}

std::shared_ptr<JSONObject> rawfrom::runCommandSingle(const std::string& zCommand) {
    auto res = org::minima::system::commands::CommandRunner::getRunner()->runSingleCommand(zCommand);
    if (!res) {
        throw std::runtime_error("CommandRunner returned null JSONObject for command: " + zCommand);
    }
    return res;
}

std::shared_ptr<JSONObject> rawfrom::anyToJSONObject(const std::any& a) {
    // Try std::shared_ptr<JSONObject>
    if (a.type() == typeid(std::shared_ptr<JSONObject>)) {
        auto ptr = std::any_cast<std::shared_ptr<JSONObject>>(a);
        if (!ptr) {
            throw std::runtime_error("Null JSONObject shared_ptr in JSONArray");
        }
        return ptr;
    }

    // Try raw pointer
    if (a.type() == typeid(JSONObject*)) {
        JSONObject* op = std::any_cast<JSONObject*>(a);
        if (!op) {
            throw std::runtime_error("Null JSONObject* in JSONArray");
        }
        return std::make_shared<JSONObject>(*op);
    }

    // Try value
    if (a.type() == typeid(JSONObject)) {
        const JSONObject& ov = std::any_cast<const JSONObject&>(a);
        return std::make_shared<JSONObject>(ov);
    }

    throw std::runtime_error("JSONArray element is not a JSONObject");
}

void rawfrom::skipWS(const std::string& s, std::size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
}

std::string rawfrom::parseJSONStringQuoted(const std::string& s, std::size_t& i) {
    // Expects s[i] == '"'
    if (i >= s.size() || s[i] != '"') {
        throw std::runtime_error("Expected '\"' at position in JSON parsing");
    }
    ++i; // skip opening quote

    std::string out;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '\\') {
            if (i >= s.size()) break;
            char e = s[i++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    // Minimal handling: read 4 hex, keep as literal \uXXXX
                    if (i + 4 <= s.size()) {
                        std::string hex = s.substr(i, 4);
                        i += 4;
                        out.append("\\u" + hex);
                    }
                    break;
                }
                default:
                    out.push_back(e);
            }
        } else if (c == '"') {
            // closing quote
            return out;
        } else {
            out.push_back(c);
        }
    }
    throw std::runtime_error("Unterminated JSON string");
}

std::string rawfrom::parseJSONValueToken(const std::string& s, std::size_t& i) {
    // Parse until ',' or '}' at top level, respecting brackets and quotes
    std::size_t start = i;
    int brace = 0;
    int bracket = 0;
    bool inStr = false;

    while (i < s.size()) {
        char c = s[i];
        if (!inStr) {
            if (c == '{') ++brace;
            else if (c == '}') {
                if (brace == 0 && bracket == 0) break;
                --brace;
            } else if (c == '[') ++bracket;
            else if (c == ']') {
                if (bracket == 0 && brace == 0) break;
                --bracket;
            } else if (c == '"') {
                inStr = true;
            } else if (c == ',') {
                if (brace == 0 && bracket == 0) break;
            }
        } else {
            if (c == '\\') {
                ++i; // skip next char
            } else if (c == '"') {
                inStr = false;
            }
        }
        ++i;
    }

    // Trim whitespace around token
    std::size_t end = i;
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::vector<std::pair<int, std::string>> rawfrom::parseFlatStateObject(const JSONObject& obj) {
    std::vector<std::pair<int, std::string>> res;

    std::string js = obj.toJSONString();
    std::size_t i = 0;
    skipWS(js, i);
    if (i >= js.size() || js[i] != '{') {
        // Not an object - nothing to add
        return res;
    }
    ++i; // skip '{'

    while (true) {
        skipWS(js, i);
        if (i >= js.size()) break;
        if (js[i] == '}') { ++i; break; }

        // Key
        if (js[i] != '"') {
            // Invalid format for our use-case; abort parsing gracefully
            break;
        }
        std::string key = parseJSONStringQuoted(js, i);

        skipWS(js, i);
        if (i >= js.size() || js[i] != ':') break;
        ++i; // skip ':'
        skipWS(js, i);

        // Value
        std::string valueStr;
        if (i < js.size() && js[i] == '"') {
            // Quoted string
            valueStr = parseJSONStringQuoted(js, i);
        } else {
            // Primitive token (number, bool, null, or nested object/array)
            valueStr = parseJSONValueToken(js, i);
        }

        // Convert key to port int if possible
        try {
            int port = std::stoi(key);
            res.emplace_back(port, valueStr);
        } catch (...) {
            // Ignore malformed port keys
        }

        skipWS(js, i);
        if (i < js.size() && js[i] == ',') {
            ++i; // next pair
            continue;
        } else if (i < js.size() && js[i] == '}') {
            ++i; // end object
            break;
        } else if (i >= js.size()) {
            break;
        } else {
            // Unexpected char - bail
            break;
        }
    }

    return res;
}

org::minima::system::commands::Command* rawfrom::getFunction() {
    return new rawfrom();
}

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org