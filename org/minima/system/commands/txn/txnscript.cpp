#include "org/minima/system/commands/txn/txnscript.hpp"

#include <utility>
#include <stdexcept>
#include <cctype>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/mmr/m_m_r_proof.hpp"

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No OS-specific behavior needed here currently.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

namespace {

// Simple JSON string parser for top-level flat objects with string keys and string values.
// Supports inputs like: {"RETURN TRUE": "", "RETURN TRUE":"0x00.."} with basic escaping handling.
class FlatStringObjectParser {
public:
    explicit FlatStringObjectParser(const std::string& s) : m_s(s), m_i(0) {}

    std::vector<std::pair<std::string, std::string>> parse() {
        skipWS();
        if (!consume('{')) {
            // Empty or invalid -> treat as empty
            return {};
        }
        skipWS();
        if (peek() == '}') {
            consume('}');
            return {};
        }
        std::vector<std::pair<std::string, std::string>> out;
        while (true) {
            skipWS();
            std::string key = parseJSONString();
            skipWS();
            if (!consume(':')) throw std::runtime_error("Invalid JSON: expected ':'");
            skipWS();
            std::string val = parseJSONString();
            out.emplace_back(std::move(key), std::move(val));
            skipWS();
            if (consume('}')) {
                break;
            }
            if (!consume(',')) throw std::runtime_error("Invalid JSON: expected ',' or '}'");
        }
        return out;
    }

private:
    const std::string& m_s;
    std::size_t m_i;

    char peek() const {
        return (m_i < m_s.size()) ? m_s[m_i] : '\0';
    }

    void skipWS() {
        while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) {
            ++m_i;
        }
    }

    bool consume(char c) {
        if (m_i < m_s.size() && m_s[m_i] == c) {
            ++m_i;
            return true;
        }
        return false;
    }

    std::string parseJSONString() {
        if (!consume('\"')) throw std::runtime_error("Invalid JSON: expected '\"'");
        std::string out;
        while (m_i < m_s.size()) {
            char c = m_s[m_i++];
            if (c == '\\') {
                if (m_i >= m_s.size()) throw std::runtime_error("Invalid JSON: incomplete escape");
                char e = m_s[m_i++];
                switch (e) {
                    case '\"': out.push_back('\"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u':
                        // For simplicity, we don't handle \uXXXX; keep as-is or skip (not expected in scripts/proofs)
                        // We'll store literally as \uXXXX if present.
                        out.append("\\u");
                        // Copy next 4 hex chars if present
                        for (int k = 0; k < 4 && m_i < m_s.size(); ++k) {
                            out.push_back(m_s[m_i++]);
                        }
                        break;
                    default:
                        // Unknown escape, keep literally
                        out.push_back(e);
                        break;
                }
            } else if (c == '\"') {
                return out;
            } else {
                out.push_back(c);
            }
        }
        throw std::runtime_error("Invalid JSON: unterminated string");
    }
};

} // anonymous namespace

txnscript::txnscript()
    : org::minima::system::commands::Command(
          "txnscript",
          "[id:] (auto:false) (scripts:{}) - Add scripts to a txn") {}

std::string txnscript::getFullHelp() const {
    return
        "\ntxnscript\n"
        "\n"
        "Add scripts to a transaction.\n"
        "\n"
        "id:\n"
        "    The id of the transaction.\n"
        "\n"
        "auto:\n"
        "    Automatically add scripts you know. Useful for multi user transactions.\n"
        "\n"
        "scripts:\n"
        "    JSON holds the script and the proof in the format {script:proof}\n"
        "    If it is a single script, and not one created with mmrcreate, leave the proof blank.\n"
        "    If it is an mmrcreate script, include the proof.\n"
        "\n"
        "Examples:\n"
        "\n"
        "txnscript id:txnmast scripts:{\"RETURN TRUE\":\"\"}\n"
        "\n"
        "txnscript id:txnmast scripts:{\"RETURN TRUE\":\"0x000..\"}\n";
}

std::vector<std::string> txnscript::getValidParams() const {
    return {"id","scripts","auto"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnscript::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::txndb::TxnDB;
    using org::minima::database::userprefs::txndb::TxnRow;
    using org::minima::database::wallet::Wallet;
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;
    using org::minima::objects::Coin;
    using org::minima::objects::ScriptProof;
    using org::minima::objects::base::MiniData;
    using org::minima::objects::mmr::MMRProof;
    using org::minima::utils::json::JSONObject;
    using org::minima::system::commands::CommandException;

    auto ret = getJSONReply();

    TxnDB* db = &MinimaDB::getDB()->getCustomTxnDB();

    // The transaction
    std::string id = getParam("id");
    JSONObject defobj;
    std::unique_ptr<JSONObject> scripts = getJSONObjectParam("scripts", defobj);
    bool autoFlag = getBooleanParam("auto", false);

    // Get the Transaction row
    TxnRow* txnrow = db->getTransactionRow(id);
    if (txnrow == nullptr) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }
    Witness& witness = txnrow->getWitness();

    std::vector<std::string> addedscripts;

    if (autoFlag) {
        // Need the transaction
        Transaction& trans = txnrow->getTransaction();

        // Get the main Wallet
        Wallet* walletdb = &MinimaDB::getDB()->getWallet();

        // Get all the inputs
        auto& inputs = trans.getAllInputs();
        for (const auto& inptr : inputs) {
            const Coin* input = inptr.get();
            if (!input) {
                continue;
            }
            const MiniData addr = input->getAddress();

            // Is the script missing
            if (witness.getScript(addr) == nullptr) {
                std::string scraddress = addr.to0xString();
                std::unique_ptr<ScriptRow> srow =
                    walletdb->getScriptFromAddress(scraddress);
                if (srow) {
                    auto pscr = std::make_unique<ScriptProof>(srow->getScript());
                    // Record the script text (avoid MiniString dependency)
                    addedscripts.emplace_back(srow->getScript());
                    witness.addScript(std::move(pscr));
                }
            }
        }
    } else {
        // Iterate over the scripts JSONObject entries: keys are script strings, values are proof strings.
        std::vector<std::pair<std::string, std::string>> kvpairs;
        try {
            FlatStringObjectParser parser(scripts ? scripts->toString() : "{}");
            kvpairs = parser.parse();
        } catch (const std::exception& e) {
            throw CommandException(std::string("Invalid scripts JSON: ") + e.what());
        }

        for (const auto& kv : kvpairs) {
            const std::string& exscript = kv.first;
            const std::string& proof = kv.second;

            if (proof.empty()) {
                auto scprf = std::make_unique<ScriptProof>(exscript);
                addedscripts.emplace_back(exscript);
                witness.addScript(std::move(scprf));
            } else {
                MiniData proofdata(proof);
                MMRProof scproof = MMRProof::convertMiniDataVersion(proofdata);
                auto scprf = std::make_unique<ScriptProof>(exscript, scproof);
                addedscripts.emplace_back(exscript);
                witness.addScript(std::move(scprf));
            }
        }
    }

    // Output the current transaction row
    ret->put("response", txnrow->toJSON());
    return ret;
}

org::minima::system::commands::Command* txnscript::getFunction() {
    return new txnscript();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org