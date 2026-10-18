#include "org/minima/system/commands/send/wallet/sendfrom.hpp"

#include <sstream>
#include <cctype>

#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {
namespace wallet {

sendfrom::sendfrom()
    : org::minima::system::commands::Command(
          "sendfrom",
          "[fromaddress:] [address:] [amount:] (tokenid:) (state:) [script:] [privatekey:] [keyuses:] (burn:) (mine:) - Send Minima or Tokens from a certain address") {}

std::vector<std::string> sendfrom::getValidParams() const {
    return {
        "fromaddress",
        "address",
        "amount",
        "tokenid",
        "script",
        "privatekey",
        "keyuses",
        "mine",
        "burn",
        "state"
    };
}

std::string sendfrom::boolToString(bool b) {
    return b ? "true" : "false";
}

std::shared_ptr<org::minima::utils::json::JSONObject> sendfrom::runCommand(const std::string& zCommand) {
    // Use single command runner for functional equivalence in this context.
    return org::minima::system::commands::CommandRunner::getRunner()->runSingleCommand(zCommand);
}

std::unique_ptr<org::minima::utils::json::JSONObject> sendfrom::runCommand() {
    using org::minima::objects::StateVariable;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::base::MiniNumber;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    // From which address
    std::string fromaddress = getAddressParam("fromaddress");
    std::string toaddress   = getAddressParam("address");

    // Amount and token id
    std::unique_ptr<MiniNumber> amount_ptr = getNumberParam("amount");
    if (!amount_ptr) {
        throw org::minima::system::commands::CommandException("Invalid or missing amount parameter");
    }
    MiniNumber amount = *amount_ptr;

    std::string tokenid = getAddressParam("tokenid", "0x00");

    // Get the BURN
    std::unique_ptr<MiniNumber> burn_ptr = getNumberParam("burn", MiniNumber::ZERO());
    MiniNumber burn = burn_ptr ? *burn_ptr : MiniNumber::ZERO();
    if (burn.isMore(MiniNumber::ZERO()) && tokenid != "0x00") {
        throw org::minima::system::commands::CommandException(
            "Currently BURN on precreated transactions only works for Minima.. tokenid:0x00.. not tokens.");
    }

    // The script of the address
    std::string script = getParam("script");

    // The private key we need to sign with
    std::string privatekey = getAddressParam("privatekey");
    std::unique_ptr<MiniNumber> keyuses_ptr = getNumberParam("keyuses");
    if (!keyuses_ptr) {
        throw org::minima::system::commands::CommandException("Invalid or missing keyuses parameter");
    }
    MiniNumber keyuses = *keyuses_ptr;

    // ID of the custom transaction
    std::string randomid = MiniData::getRandomData(32).to0xString();

    // Are we mining
    bool mine = getBooleanParam("mine", true);

    // Is there a state
    std::unique_ptr<JSONObject> state_obj;
    if (existsParam("state")) {
        state_obj = getJSONObjectParam("state");
    }

    // Now construct the transaction..
    std::shared_ptr<JSONObject> result = runCommand("txncreate id:" + randomid);

    // Add the amounts..
    // Note: keep exact spacing after "fromaddress:" as in Java string literal ("fromaddress: ").
    std::string command = "txnaddamount id:" + randomid +
                          " burn:" + burn.toString() +
                          " fromaddress: " + fromaddress +
                          " address:" + toaddress +
                          " amount:" + amount.toString() +
                          " tokenid:" + tokenid;
    result = runCommand(command);

    bool status_ok = false;
    try {
        status_ok = result->getBoolean("status");
    } catch (...) {
        status_ok = false;
    }

    if (!status_ok) {
        // Delete transaction
        runCommand("txndelete id:" + randomid);

        // Not enough funds or generic error
        std::string err;
        try {
            err = result->getString("error");
        } catch (...) {
            err = "Unknown error during txnaddamount";
        }
        throw org::minima::system::commands::CommandException(err);
    }

    // Add the scripts..
    runCommand("txnscript id:" + randomid + " scripts:{\"" + script + "\":\"\"}");

    // Add the state if exists
    if (state_obj) {
        // Parse the flat JSONObject string and iterate over key/value
        const std::string json_state = state_obj->toJSONString();
        auto pairs = parseFlatJSONObjectString(json_state);

        for (const auto& kv : pairs) {
            const std::string& portstr = kv.first;
            const std::string& var     = kv.second;

            // The port
            int port = 0;
            try {
                port = std::stoi(portstr);
            } catch (...) {
                throw org::minima::system::commands::CommandException("Invalid state port: " + portstr);
            }

            // Create a state variable
            StateVariable sv(port, var);

            // Add to the transaction (always quote value, matching Java)
            runCommand("txnstate id:" + randomid +
                       " port:" + std::to_string(sv.getPort()) +
                       " value:\"" + sv.getData().toString() + "\"");
        }
    }

    // Sort the MMR
    runCommand("txnmmr id:" + randomid);

    // Now SIGN
    runCommand("txnsign id:" + randomid +
               " publickey:custom privatekey:" + privatekey +
               " keyuses:" + keyuses.toString());

    // And POST!
    result = runCommand("txnpost id:" + randomid + " mine:" + boolToString(mine));

    // And delete..
    runCommand("txndelete id:" + randomid);

    // And return..
    try {
        ret->put("response", result->get("response"));
    } catch (...) {
        // If "response" missing, keep behavior safe by returning the whole result as string
        ret->put("response", result->toJSONString());
    }

    return ret;
}

std::string sendfrom::unescapeJSONString(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '\\' && (i + 1) < in.size()) {
            char n = in[++i];
            switch (n) {
                case '\\': out.push_back('\\'); break;
                case '"':  out.push_back('"');  break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                default:
                    // Fallback: keep as-is
                    out.push_back(n);
                    break;
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> sendfrom::parseFlatJSONObjectString(const std::string& json) {
    std::vector<std::pair<std::string, std::string>> result;

    // Simple state machine parser for a flat JSON object
    enum class State { Start, Key, PostKey, PreValue, ValueString, ValueBare, PostValue, Done };

    size_t i = 0, n = json.size();
    auto skip_ws = [&](void) {
        while (i < n && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
    };

    skip_ws();
    if (i >= n || json[i] != '{') return result;
    ++i;

    while (true) {
        skip_ws();
        if (i < n && json[i] == '}') { ++i; break; } // empty object

        // Read key string
        if (i >= n || json[i] != '"') {
            // Not a proper JSON; stop parsing safely
            break;
        }
        ++i; // skip quote
        std::string key;
        while (i < n) {
            char c = json[i++];
            if (c == '\\') {
                if (i < n) key.push_back(json[i++]); // keep escaped char raw; we only need numeric key
            } else if (c == '"') {
                break;
            } else {
                key.push_back(c);
            }
        }
        key = unescapeJSONString(key);

        // Colon
        skip_ws();
        if (i >= n || json[i] != ':') break;
        ++i;

        // Read value
        skip_ws();
        std::string value;
        if (i < n && json[i] == '"') {
            // String value
            ++i; // skip quote
            std::string vraw;
            while (i < n) {
                char c = json[i++];
                if (c == '\\') {
                    if (i < n) vraw.push_back('\\'), vraw.push_back(json[i++]); // keep escape for unescape step
                } else if (c == '"') {
                    break;
                } else {
                    vraw.push_back(c);
                }
            }
            value = unescapeJSONString(vraw);
        } else {
            // Bare value: read until ',' or '}'
            size_t start = i;
            int brace_depth = 0; // we assume flat, but be safe with primitives
            while (i < n) {
                char c = json[i];
                if (c == '{' || c == '[') brace_depth++;
                if (c == '}' || c == ']') {
                    if (brace_depth == 0) break;
                    brace_depth--;
                }
                if (brace_depth == 0 && (c == ',' || c == '}')) break;
                ++i;
            }
            size_t end = i;
            // trim whitespace
            while (start < end && std::isspace(static_cast<unsigned char>(json[start]))) ++start;
            while (end > start && std::isspace(static_cast<unsigned char>(json[end - 1]))) --end;
            value = json.substr(start, end - start);
        }

        result.emplace_back(key, value);

        // Next: either ',' or '}'
        skip_ws();
        if (i < n && json[i] == ',') {
            ++i;
            continue;
        } else if (i < n && json[i] == '}') {
            ++i;
            break;
        } else {
            break; // invalid or end
        }
    }

    return result;
}

org::minima::system::commands::Command* sendfrom::getFunction() {
    return new sendfrom();
}

} // namespace wallet
} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org