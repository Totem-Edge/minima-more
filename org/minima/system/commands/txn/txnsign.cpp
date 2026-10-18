#include "org/minima/system/commands/txn/txnsign.hpp"

#include <memory>
#include <utility>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/backup/vault.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/tx_po_w.hpp"

#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/keys/tree_key.hpp"

#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/commands/txn/txnpost.hpp" // Assumed available

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::txndb::TxnDB;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::database::wallet::Wallet;
using org::minima::database::wallet::KeyRow;
// using org::minima::database::wallet::ScriptRow;

using org::minima::objects::Transaction;
using org::minima::objects::Witness;
using org::minima::objects::Coin;

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

using org::minima::objects::keys::Signature;
using org::minima::objects::keys::TreeKey;

using org::minima::system::brains::TxPoWGenerator;

using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;

txnsign::txnsign()
    : org::minima::system::commands::Command(
          "txnsign",
          "[id:] [publickey:0x..|auto] (txnpostauto:) (txnpostburn:) (txnpostmine:) (txndelete:) - Sign a transaction") {}

std::string txnsign::getFullHelp() const {
    return std::string("\ntxnsign\n"
                       "\n"
                       "Sign a transaction.\n"
                       "\n"
                       "Specify the public key or use 'auto' if the coin inputs are simple.\n"
                       "\n"
                       "id:\n"
                       "    The id of the transaction to sign.\n"
                       "\n"
                       "publickey:\n"
                       "    The public key specified in a custom script, or 'auto' for transactions with simple inputs.\n"
                       "\n"
                       "txnpostauto: (optional)\n"
                       "    Do you want to post this transaction. Use the same values as you would for txnpost auto(sort MMR and Scripts)\n"
                       "\n"
                       "txnpostburn: (optional)\n"
                       "    If you also post this transaction, do you want to add a burn transaction.\n"
                       "\n"
                       "txnpostmine: (optional)\n"
                       "    If you also post this transaction, do you want to mine it immediately.\n"
                       "\n"
                       "txndelete: (optional)\n"
                       "    true or false - delete this txn after signing AND posting.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "txnsign id:simpletxn publickey:auto\n"
                       "\n"
                       "txnsign id:simpletxn publickey:auto password:your_password\n"
                       "\n"
                       "txnsign id:multisig publickey:0xFD8B..\n"
                       "\n"
                       "txnsign id:simpletxn publickey:auto txnpostauto:true\n");
}

std::vector<std::string> txnsign::getValidParams() const {
    return std::vector<std::string>{
        "id", "publickey", "txndelete", "txnpostauto", "txnpostburn",
        "txnpostmine", "password", "privatekey", "keyuses"};
}

std::unique_ptr<JSONObject> txnsign::runCommand() {
    auto ret = getJSONReply();

    //
    // FIX 1: Change from pointer (*) to reference (&)
    //
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    // Required params
    const std::string id   = getParam("id");
    const std::string pubk = getParam("publickey");

    //
    // FIX 2: Change from arrow (->) to dot (.)
    //
    TxnRow* txnrow = db.getTransactionRow(id);
    if (!txnrow) {
        throw org::minima::system::commands::CommandException("Transaction not found : " + id);
    }

    // Get references to Transaction and Witness
    Transaction& txn = txnrow->getTransaction();
    Witness& wit     = txnrow->getWitness();

    // Precompute CoinIDs for outputs
    TxPoWGenerator::precomputeTransactionCoinID(txn);

    // Calculate Transaction ID
    txn.calculateTransactionID();

    //
    // FIX 1: Change from pointer (*) to reference (&)
    //
    Wallet& walletdb = MinimaDB::getDB()->getWallet();

    JSONArray notfoundkeys;
    JSONArray foundkeys;
    JSONArray nonsimplekeys;

    JSONObject resp;

    bool passwordlock = false;
    //
    // FIX 2: Change from arrow (->) to dot (.)
    //
    if (existsParam("password") && !walletdb.isBaseSeedAvailable()) {
        // Unlock the DB
        org::minima::system::commands::backup::vault::passowrdUnlockDB(getParam("password"));
        passwordlock = true;
    }

    if (pubk == "auto") {
        // Auto-sign for simple inputs
        auto& inputs = txn.getAllInputs();
        for (const auto& ccptr : inputs) {
            if (!ccptr) continue;
            const Coin& cc = *ccptr;

            //
            // FIX 2: Change from arrow (->) to dot (.)
            //
            std::unique_ptr<ScriptRow> scrow = walletdb.getScriptFromAddress(cc.getAddress().to0xString());
            if (!scrow) {
                notfoundkeys.add(cc.getAddress().to0xString());
                continue;
            } else if (!scrow->isSimple()) {
                nonsimplekeys.add(scrow->getAddress());
                continue;
            }

            // Skip if already signed
            if (!wit.isSignedBy(scrow->getPublicKey())) {
                // Record found key
                foundkeys.add(scrow->getPublicKey());

                //
                // FIX 2: Change from arrow (->) to dot (.)
                //
                std::unique_ptr<Signature> signature = walletdb.signData(scrow->getPublicKey(), txn.getTransactionID());
                wit.addSignature(std::move(signature));
            }
        }
    } else if (pubk == "custom") {
        // Custom private key and uses
        std::unique_ptr<MiniData> privkey = getDataParam("privatekey");
        std::unique_ptr<MiniNumber> uses  = getNumberParam("keyuses");

        // Create TreeKey from private seed
        TreeKey treekey = TreeKey::createDefault(*privkey);

        // Set uses
        treekey.setUses(uses->getAsInt());

        // Sign the TxnID
        Signature signature = treekey.sign(txn.getTransactionID());

        // Add to witness
        auto sigptr = std::make_unique<Signature>(std::move(signature));
        wit.addSignature(std::move(sigptr));
    } else {
        //
        // FIX 2: Change from arrow (->) to dot (.)
        //
        std::unique_ptr<KeyRow> pubrow = walletdb.getKeyFromPublic(pubk);
        if (!pubrow) {
            if (passwordlock) {
                org::minima::system::commands::backup::vault::passwordLockDB(getParam("password"));
            }
            throw org::minima::system::commands::CommandException("Public Key not found : " + pubk);
        }

        // Record found key
        foundkeys.add(pubrow->getPublicKey());

        //
        // FIX 2: Change from arrow (->) to dot (.)
        //
        std::unique_ptr<Signature> signature =
            walletdb.signData(pubrow->getPublicKey(), txn.getTransactionID());

        // Add it
        wit.addSignature(std::move(signature));
    }

    // Re-lock the DB if we unlocked it
    if (passwordlock) {
        org::minima::system::commands::backup::vault::passwordLockDB(getParam("password"));
    }

    // Keys used
    resp.put("keys", foundkeys);

    // Non-simple keys encountered
    if (nonsimplekeys.size() > 0) {
        resp.put("nonsimple", nonsimplekeys);
    }

    // Not found keys (addresses)
    if (notfoundkeys.size() > 0) {
        resp.put("notfound", notfoundkeys);
    }

    // Optional post
    if (existsParam("txnpostauto")) {
        const bool postauto = getBooleanParam("txnpostauto");
        std::unique_ptr<MiniNumber> burn = getNumberParam("txnpostburn", org::minima::objects::base::MiniNumber::ZERO());
        const bool minesync = getBooleanParam("txnpostmine", false);

        auto txp_ptr =
            org::minima::system::commands::txn::txnpost::postTxn(id, *burn, postauto, minesync);

        resp.put("txnpost", true);
        resp.put("txnpostauto", postauto);
        resp.put("txnpostburn", burn->toString());
        resp.put("txnpostmine", minesync);
        
        //
        // FIX 3 (Related): Use dot (.) operator on the TxPoW object
        //
        resp.put("txpow", txp_ptr->toJSON());

        // Autodelete after posting if requested
        const bool autodelete = getBooleanParam("txndelete", false);
        if (autodelete) {
            //
            // FIX 2: Change from arrow (->) to dot (.)
            //
            bool found = db.deleteTransaction(id);
            (void)found;
            resp.put("delete", true);
        } else {
            resp.put("delete", false);
        }
    } else {
        resp.put("txnpost", false);
    }

    // Set response
    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* txnsign::getFunction() {
    return new txnsign();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
