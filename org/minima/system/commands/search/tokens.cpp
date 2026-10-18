#include "org/minima/system/commands/search/tokens.hpp"

#include <memory>
#include <string>
#include <vector>

#include "org/minima/objects/token.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace search {

using org::minima::objects::Token;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;

tokens::tokens()
    : org::minima::system::commands::Command(
          "tokens",
          "(tokenid:) (action:import|export) (data:) - List, import or export tokens on the chain") {}

std::string tokens::getFullHelp() const {
    return std::string("\ntokens\n")
         + "\n"
         + "List all tokens in the unpruned chain.\n"
         + "\n"
         + "Optionally import or export tokens to share token data.\n"
         + "\n"
         + "tokenid: (optional)\n"
         + "    The tokenid of the token to search for or export.\n"
         + "\n"
         + "action: (optional)\n"
         + "    import : List your existing public keys.\n"
         + "    export : Create a new key.\n"
         + "\n"
         + "data: (optional)\n"
         + "    The data of the token to import, generated from the export.\n"
         + "\n"
         + "Examples:\n"
         + "\n"
         + "tokens\n"
         + "\n"
         + "tokens tokenid:0xFED5..\n"
         + "\n"
         + "tokens action:export tokenid:0xFED5..\n"
         + "\n"
         + "tokens action:import data:0x000..\n";
}

std::vector<std::string> tokens::getValidParams() const {
    return std::vector<std::string>{ "tokenid", "action", "data" };
}

std::unique_ptr<JSONObject> tokens::runCommand() {
    auto ret = getJSONReply();

    const std::string tokenid = getParam("tokenid", "");
    const std::string action  = getParam("action", "");

    if (action == "export") {
        // Export a token..
        std::shared_ptr<Token> tok = TxPoWSearcher::getToken(MiniData(tokenid));
        if (!tok) {
            throw org::minima::system::commands::CommandException("Token not found : " + tokenid);
        }

        // Convert to MiniData..
        std::unique_ptr<MiniData> tokdata = MiniData::getMiniDataVersion(*tok);
        if (!tokdata) {
            throw org::minima::system::commands::CommandException("Failed to serialize token for export");
        }

        JSONObject resp;
        resp.put("tokenid", tokenid);
        resp.put("data", tokdata->to0xString());
        ret->put("response", resp);

    } else if (action == "import") {
        const std::string data = getParam("data");
        MiniData tokendata(data);
        std::unique_ptr<Token> newtok = Token::convertMiniDataVersion(tokendata);
        if (!newtok) {
            throw org::minima::system::commands::CommandException("Invalid token data");
        }

        // Add this..
        std::shared_ptr<Token> newtok_shared(std::move(newtok));
        TxPoWSearcher::importToken(newtok_shared);

        JSONObject resp;
        std::unique_ptr<JSONObject> tokjson = newtok_shared->toJSON();
        resp.put("token", *tokjson);
        ret->put("response", resp);

    } else {
        if (tokenid.empty()) {
            // The return array
            JSONArray toksarr;

            // First add Minima..
            JSONObject minima;
            minima.put("name", std::string("Minima"));
            minima.put("tokenid", std::string("0x00"));
            minima.put("total", std::string("1000000000"));
            minima.put("decimals", MiniNumber::MAX_DECIMAL_PLACES);
            minima.put("scale", 1);
            toksarr.add(minima);

            // Get ALL the tokens in the chain..
            std::vector<std::shared_ptr<Token>> alltokens = TxPoWSearcher::getAllTokens();

            for (const std::shared_ptr<Token>& tok : alltokens) {
                // Add to our list
                std::unique_ptr<JSONObject> tj = tok->toJSON();
                toksarr.add(*tj);
            }

            ret->put("response", toksarr);

        } else {
            // Search for one token..
            std::shared_ptr<Token> tok = TxPoWSearcher::getToken(MiniData(tokenid));
            if (!tok) {
                throw org::minima::system::commands::CommandException("Token not found : " + tokenid);
            }
            std::unique_ptr<JSONObject> tj = tok->toJSON();
            ret->put("response", *tj);
        }
    }

    return ret;
}

org::minima::system::commands::Command* tokens::getFunction() {
    return new tokens();
}

} // namespace search
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org