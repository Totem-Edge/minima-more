#include "org/minima/system/commands/base/tokenvalidate.hpp"

#include <any>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"

#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"

#include "org/minima/utils/r_p_c_client.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"

namespace {

std::string trim_copy(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    if (start == s.size()) return std::string();
    std::size_t end = s.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end]))) {
        --end;
    }
    return s.substr(start, end - start + 1);
}

} // anonymous namespace

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::keys::Signature;
using org::minima::objects::keys::TreeKey;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::system::commands::CommandException;
using org::minima::utils::RPCClient;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::parser::JSONParser;

tokenvalidate::tokenvalidate()
    : Command("tokenvalidate", "[tokenid:] - validate the signature and web link in a token") {}

std::string tokenvalidate::getFullHelp() const {
    return "\ntokenvalidate\n"
           "\n"
           "Validate the signature and webvalidate link in a token.\n"
           "\n"
           "tokenid:\n"
           "    The tokenid of the custom token/NFT to validate.\n"
           "\n"
           "Examples:\n"
           "\n"
           "tokenvalidate tokenid:0xFED5..\n";
}

std::vector<std::string> tokenvalidate::getValidParams() const {
    return std::vector<std::string>{"tokenid"};
}

std::unique_ptr<JSONObject> tokenvalidate::runCommand() {
    // Reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Get the tokenid
    std::string tokenid = getParam("tokenid");

    // Is it Minima..
    if (tokenid == "0x00") {
        throw CommandException("The Main Minima Token 0x00 has no signature");
    }

    // Get ALL the tokens in the chain..
    std::vector<std::shared_ptr<Token>> alltokens = TxPoWSearcher::getAllTokens();

    // Now get the correct token
    std::shared_ptr<Token> tok;
    for (const auto& token : alltokens) {
        // Java code assumes getTokenID() is non-null
        const org::minima::objects::base::MiniData* tid = token->getTokenID();
        if (tid && tid->to0xString() == tokenid) {
            tok = token;
            break;
        }
    }

    // Did we find it
    if (!tok) {
        throw CommandException(std::string("Token not found : ") + tokenid);
    }

    // Now validate..
    // Parse the token name string as JSONObject
    std::string namestr = tok->getName().toString();
    JSONParser parser;
    std::any parsed = parser.parse(namestr);

    JSONObject description;
    bool desc_ok = false;
    try {
        auto pobj = std::any_cast<std::shared_ptr<JSONObject>>(parsed);
        if (pobj) {
            description = *pobj;
            desc_ok = true;
        }
    } catch (const std::bad_any_cast&) {
        // fall through
    }
    if (!desc_ok) {
        try {
            description = std::any_cast<JSONObject>(parsed);
            desc_ok = true;
        } catch (const std::bad_any_cast&) {
            throw CommandException("Token name JSON parse did not produce a JSONObject");
        }
    }

    JSONObject resp;

    // Does it have a signature..
    if (description.containsKey("signature") && description.containsKey("signedby")) {
        JSONObject sign;
        sign.put("signed", true);

        // get the signature
        std::string sigstr = description.getString("signature");
        MiniData sigdata(sigstr);
        std::unique_ptr<Signature> sig = Signature::convertMiniDataVersion(sigdata);
        MiniData root = sig->getRootPublicKey();

        // Who signed it..
        std::string signedby = description.getString("signedby");
        MiniData pubkey(signedby);
        if (!pubkey.isEqual(root)) {
            sign.put("valid", false);
            sign.put("reason", std::string("Public key does not match signedby : ") + root.toString());
        } else {
            sign.put("signedby", pubkey.to0xString());

            // Get the coinid of the token
            const MiniData& coinid = tok->getCoinID();

            // Verify the signature..
            TreeKey tk;
            tk.setPublicKey(sig->getRootPublicKey());

            // Check the signature
            bool valid = tk.verify(coinid, *sig);
            sign.put("valid", valid);
            if (!valid) {
                sign.put("reason", "Signature fails");
            }
        }

        resp.put("signature", sign);
    } else {
        JSONObject sign;
        sign.put("signed", false);
        resp.put("signature", sign);
    }

    // Does it have a web URL
    if (description.containsKey("webvalidate")) {
        JSONObject wval;
        wval.put("webvalidate", true);

        // Get the URL
        std::string url = description.getString("webvalidate");
        wval.put("url", url);

        // Now load that file..
        try {
            // Get the value
            std::string val = trim_copy(RPCClient::sendGET(url));

            // Do they match
            const MiniData* tid = tok->getTokenID();
            std::string tokenidhex = tid ? tid->to0xString() : std::string();
            if (val == tokenidhex) {
                wval.put("valid", true);
            } else {
                wval.put("valid", false);
                wval.put("reason", std::string("Data in file does not match tokenid : ") + val);
            }
        } catch (const std::exception& exc) {
            // Something gone wrong
            wval.put("valid", false);
            wval.put("reason", std::string("Could not download web file : ") + exc.what());
        }

        resp.put("web", wval);
    } else {
        JSONObject wval;
        wval.put("webvalidate", false);
        resp.put("web", wval);
    }

    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* tokenvalidate::getFunction() {
    return new tokenvalidate();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org