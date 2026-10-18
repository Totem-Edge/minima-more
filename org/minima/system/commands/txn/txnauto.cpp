#include "org/minima/system/commands/txn/txnauto.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"

#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

namespace {

// Attach ScriptProofs for all unique input addresses of a transaction
static void attachScriptsOnly(org::minima::objects::Transaction& transaction,
                              org::minima::objects::Witness& witness) {
    using namespace org::minima;

    // Clear existing script proofs to mirror auto-generation behavior
    witness.clearScriptProofs();

    auto* db = database::MinimaDB::getDB();
    auto& wallet = db->getWallet();

    const auto& inputs = transaction.getAllInputs();
    if (inputs.empty()) {
        return;
    }

    std::unordered_set<std::string> added; // avoid duplicate scripts for same address

    for (const auto& inptr : inputs) {
        if (!inptr) {
            continue;
        }

        std::string scraddress = inptr->getAddress().to0xString();
        if (added.find(scraddress) != added.end()) {
            continue;
        }

        std::unique_ptr<ScriptRow> srow = wallet.getScriptFromAddress(scraddress);
        if (!srow) {
            throw system::commands::CommandException("SERIOUS ERROR script missing for simple address : " + scraddress);
        }

        auto pscr = std::make_unique<objects::ScriptProof>(srow->getScript());
        witness.addScript(std::move(pscr));

        added.insert(scraddress);
    }
}

} // anonymous namespace

txnauto::txnauto()
    : org::minima::system::commands::Command(
          "txnauto",
          "[id:] [amount:] [address:] (tokenid:) (sign:) (burn:) (mmrscript:) - Create a transaction automatically") {}

std::vector<std::string> txnauto::getValidParams() const {
    return {"id","amount","address","tokenid","sign","burn","mmrscript"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnauto::runCommand() {
    using namespace org::minima;

    auto ret = getJSONReply();

    // DB
    auto* db = database::MinimaDB::getDB();
    auto& txndb = db->getCustomTxnDB();

    // Params
    std::string id       = getParam("id");
    std::string address  = getAddressParam("address");
    std::unique_ptr<objects::base::MiniNumber> amount = getNumberParam("amount");
    std::string tokenid  = getAddressParam("tokenid", "0x00");
    bool sign            = getBooleanParam("sign", false);
    bool mmrscript       = getBooleanParam("mmrscript", true);

    // Burn
    std::unique_ptr<objects::base::MiniNumber> burn =
        getNumberParam("burn", objects::base::MiniNumber::ZERO());

    if (burn->isMore(objects::base::MiniNumber::ZERO()) && tokenid != "0x00") {
        throw system::commands::CommandException(
            "Currently BURN on precreated transactions only works for Minima.. tokenid:0x00.. not tokens.");
    }

    if (txndb.getTransactionRow(id) != nullptr) {
        throw system::commands::CommandException("Txn with this ID already exists : " + id);
    }

    // Create the txn
    txndb.createTransaction(id);

    // Add the amounts via command runner
    std::string command = "txnaddamount id:" + id +
                          " burn:" + burn->toString() +
                          " address:" + address +
                          " amount:" + amount->toString() +
                          " tokenid:" + tokenid;

    std::unique_ptr<system::commands::CommandRunner> runner = system::commands::CommandRunner::getRunner();
    std::unique_ptr<utils::json::JSONObject> result = runner->runSingleCommand(command);
    if (!result->getBoolean("status")) {
        // Delete the txn on failure
        txndb.deleteTransaction(id);
        throw system::commands::CommandException(result->getString("error"));
    }

    // Now sort the scripts and MMR
    auto* txnrow = txndb.getTransactionRow(id);
    if (!txnrow) {
        // Should not happen
        txndb.deleteTransaction(id);
        throw system::commands::CommandException("Internal error: transaction row missing.");
    }

    // Get the Transaction and Witness
    objects::Transaction& trans = txnrow->getTransaction();
    objects::Witness& wit       = txnrow->getWitness();

    // Set the MMR data and Scripts if requested
    if (mmrscript) {
        // First, set MMR proofs via the existing command
        command = "txnmmr id:" + id;
        result = runner->runSingleCommand(command);
        if (!result->getBoolean("status")) {
            txndb.deleteTransaction(id);
            throw system::commands::CommandException(result->getString("error"));
        }

        // Then attach scripts for each input
        attachScriptsOnly(trans, wit);
    }

    // Do we sign
    if (sign) {
        command = "txnsign id:" + id + " publickey:auto";
        result = runner->runSingleCommand(command);
        if (!result->getBoolean("status")) {
            // Delete the txn
            txndb.deleteTransaction(id);
            throw system::commands::CommandException(result->getString("error"));
        }
    }

    // Output the current trans
    ret->put("response", txnrow->toJSON());

    return ret;
}

org::minima::system::commands::Command* txnauto::getFunction() {
    return new txnauto();
}

std::unique_ptr<org::minima::database::userprefs::txndb::TxnRow>
txnauto::createTransaction(const std::string& zAddress,
                           const org::minima::objects::base::MiniNumber& zAmount) {
    using namespace org::minima;

    // We'll construct a temporary transaction inside the TxnDB using commands,
    // then deep-copy it out and delete the DB entry, matching Java's behavior
    // of returning an in-memory TxnRow.

    // Generate a temporary ID
    std::string tempid = "temp_" + objects::base::MiniData::getRandomData(8).to0xString();

    auto* db = database::MinimaDB::getDB();
    auto& txndb = db->getCustomTxnDB();

    // Create command runner
    std::unique_ptr<system::commands::CommandRunner> runner = system::commands::CommandRunner::getRunner();

    // Ensure any pre-existing is removed
    if (txndb.getTransactionRow(tempid) != nullptr) {
        txndb.deleteTransaction(tempid);
    }

    // Create a new transaction row
    {
        std::unique_ptr<utils::json::JSONObject> res = runner->runSingleCommand("txncreate id:" + tempid);
        if (!res->getBoolean("status")) {
            throw system::commands::CommandException(res->getString("error"));
        }
    }

    // Add inputs as a pure burn equal to zAmount (amount:0, burn:zAmount) for Minima 0x00
    {
        std::string cmd = "txnaddamount id:" + tempid +
                          " burn:" + zAmount.toString() +
                          " address:" + zAddress +
                          " amount:" + std::string("0") +
                          " tokenid:" + std::string("0x00");
        std::unique_ptr<utils::json::JSONObject> res = runner->runSingleCommand(cmd);
        if (!res->getBoolean("status")) {
            // Cleanup
            txndb.deleteTransaction(tempid);
            throw system::commands::CommandException(res->getString("error"));
        }
    }

    // Attach MMR proofs
    {
        std::unique_ptr<utils::json::JSONObject> res = runner->runSingleCommand("txnmmr id:" + tempid);
        if (!res->getBoolean("status")) {
            txndb.deleteTransaction(tempid);
            throw system::commands::CommandException(res->getString("error"));
        }
    }

    // Attach scripts from wallet based on inputs
    {
        auto* row = txndb.getTransactionRow(tempid);
        if (!row) {
            txndb.deleteTransaction(tempid);
            throw system::commands::CommandException("Internal error: transaction row missing.");
        }

        objects::Transaction& transaction = row->getTransaction();
        objects::Witness& witness         = row->getWitness();

        attachScriptsOnly(transaction, witness);
    }

    // Sign with publickey:auto
    {
        std::unique_ptr<utils::json::JSONObject> res =
            runner->runSingleCommand("txnsign id:" + tempid + " publickey:auto");
        if (!res->getBoolean("status")) {
            txndb.deleteTransaction(tempid);
            throw system::commands::CommandException(res->getString("error"));
        }
    }

    // Deep-copy TxnRow via MiniData stream and delete the temporary row
    std::unique_ptr<database::userprefs::txndb::TxnRow> outrow;
    {
        auto* row = txndb.getTransactionRow(tempid);
        if (!row) {
            txndb.deleteTransaction(tempid);
            throw system::commands::CommandException("Internal error: transaction row missing after sign.");
        }

        std::unique_ptr<objects::base::MiniData> md = objects::base::MiniData::getMiniDataVersion(*row);
        if (!md) {
            txndb.deleteTransaction(tempid);
            throw system::commands::CommandException("Failed to serialize TxnRow for deep copy.");
        }

        outrow = database::userprefs::txndb::TxnRow::convertMiniDataVersion(*md);
        if (!outrow) {
            txndb.deleteTransaction(tempid);
            throw system::commands::CommandException("Failed to deserialize TxnRow for deep copy.");
        }
    }

    // Cleanup
    txndb.deleteTransaction(tempid);

    return outrow;
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org