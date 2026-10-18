#include "org/minima/system/commands/txn/txncheck.hpp"

#include <algorithm>
#include <unordered_set>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"

#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"

#include "org/minima/system/brains/tx_po_w_checker.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

// Helper function to normalize token IDs
namespace {
    inline org::minima::objects::base::MiniData normalizeTokenID(
        const org::minima::objects::base::MiniData& tokenhash) {
        return tokenhash.isEqual(org::minima::objects::Token::TOKENID_CREATE)
            ? org::minima::objects::Token::TOKENID_MINIMA
            : tokenhash;
    }
}

txncheck::txncheck()
    : org::minima::system::commands::Command("txncheck", "[id:] - Show details about the transaction") {}

std::string txncheck::getFullHelp() const {
    return std::string("\ntxncheck\n"
                       "\n"
                       "Show details about the transaction.\n"
                       "\n"
                       "Verify whether the inputs, outputs, signatures, proofs and scripts are valid.\n"
                       "\n"
                       "id: (optional)\n"
                       "    The id of the transaction to check.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "txncheck id:multisig\n");
}

std::vector<std::string> txncheck::getValidParams() const {
    return std::vector<std::string>{"id"};
}

std::unique_ptr<JSONObject> txncheck::runCommand() {
    // Base reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    org::minima::database::userprefs::txndb::TxnDB& db =
        org::minima::database::MinimaDB::getDB()->getCustomTxnDB();

    // id parameter (throws if missing/blank)
    const std::string& id = getParam("id");

    org::minima::database::userprefs::txndb::TxnRow* txnrow = db.getTransactionRow(id);
    if (!txnrow) {
        throw org::minima::system::commands::CommandException(std::string("Transaction not found : ") + id);
    }

    // Direct references for convenience
    org::minima::objects::Transaction& txn = txnrow->getTransaction();
    org::minima::objects::Witness& wit     = txnrow->getWitness();

    // Inputs and outputs (vectors of unique_ptr)
    auto& inputs  = txn.getAllInputs();
    auto& outputs = txn.getAllOutputs();

    // Build details JSON
    JSONObject details;

    // Collect all token IDs present in inputs and outputs (normalize CREATE -> MINIMA)
    // Use unordered_set for O(1) lookups instead of O(n)
    std::unordered_set<std::string> tokens_set;
    tokens_set.reserve(inputs.size() + outputs.size());

    auto add_token = [&tokens_set](const org::minima::objects::Coin& cc) {
        org::minima::objects::base::MiniData tokenhash = normalizeTokenID(cc.getTokenID());
        tokens_set.insert(tokenhash.to0xString());
    };

    for (const auto& uptr : inputs) {
        add_token(*uptr);
    }
    for (const auto& uptr : outputs) {
        add_token(*uptr);
    }

    std::vector<std::string> tokens(tokens_set.begin(), tokens_set.end());

    // For each token, compute input total, output total, and difference
    JSONArray alltokens;
    for (const std::string& token : tokens) {
        org::minima::objects::base::MiniData tok(token);

        org::minima::objects::base::MiniNumber outamt = txn.sumOutputs(tok);
        org::minima::objects::base::MiniNumber inamt  = txn.sumInputs(tok);

        JSONObject tokcoin;
        tokcoin.put("tokenid", token);
        tokcoin.put("input", inamt.toString());
        tokcoin.put("output", outamt.toString());
        tokcoin.put("difference", inamt.sub(outamt).toString());

        alltokens.add(tokcoin);
    }
    details.put("coins", alltokens);

    // Basic counts
    details.put("tokens", static_cast<int>(tokens.size()));
    details.put("inputs", static_cast<int>(inputs.size()));
    details.put("mmrproofs", static_cast<int>(wit.getAllCoinProofs().size()));
    details.put("scripts", static_cast<int>(wit.getAllScripts().size()));

    bool correctmmrnum = (inputs.size() == wit.getAllCoinProofs().size());

    // Sum total minima amounts
    org::minima::objects::base::MiniNumber totminimain = org::minima::objects::base::MiniNumber::ZERO();
    for (const auto& uptr : inputs) {
        totminimain = totminimain.add(uptr->getAmount());
    }

    details.put("outputs", static_cast<int>(outputs.size()));

    JSONArray allouts;
    org::minima::objects::base::MiniNumber totminimaout = org::minima::objects::base::MiniNumber::ZERO();
    for (const auto& uptr : outputs) {
        totminimaout = totminimaout.add(uptr->getAmount());
        allouts.add(uptr->toJSON());
    }
    details.put("alloutputs", allouts);

    org::minima::objects::base::MiniNumber diff = totminimain.sub(totminimaout);
    details.put("burn", diff.toString());
    details.put("validamounts", txn.checkValid());

    int sigs = static_cast<int>(wit.getAllSignatures().size());
    details.put("signatures", sigs);

    std::shared_ptr<org::minima::database::txpowtree::TxPoWTreeNode> tip =
        org::minima::database::MinimaDB::getDB()->getTxPoWTree().getTip();

    auto temp_ptr = org::minima::system::brains::TxPoWGenerator::generateTxPoW(txn, wit);

    // Redo checks: reset monotonic flag on transaction
    txn.clearIsMonotonic();

    // Call checkTxPoWBasic with a const reference
    bool validbasic = org::minima::system::brains::TxPoWChecker::checkTxPoWBasic(*temp_ptr);

    const bool validsignatures = org::minima::system::brains::TxPoWChecker::checkSignatures(*temp_ptr);

    // MMR and script checks
    bool validmmr = false;
    bool validscripts = false;
    if (tip) {
        //
        // FIX: Call getMMR() to get the std::shared_ptr<MMR>
        //
        validmmr = correctmmrnum &&
           org::minima::system::brains::TxPoWChecker::checkMMR(tip->getMMR(), *temp_ptr);
        
        //
        // FIX: Call getMMR() to get the std::shared_ptr<MMR>
        //
        validscripts = org::minima::system::brains::TxPoWChecker::checkTxPoWScripts(
            tip->getMMR(), *temp_ptr, tip->getTxPoW());
    } else {
        validmmr = false;
        validscripts = false;
    }

    JSONObject valid;
    valid.put("basic", validbasic);
    valid.put("signatures", validsignatures);
    valid.put("mmrproofs", validmmr);
    valid.put("scripts", validscripts);

    details.put("valid", valid);

    // Final validity
    details.put("validtransaction", validbasic && validsignatures && validmmr && validscripts);

    // Wrap up
    ret->put("response", details);
    return ret;
}

org::minima::system::commands::Command* txncheck::getFunction() {
    return new txncheck();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
