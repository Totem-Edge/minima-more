#include "org/minima/system/commands/send/sendsign.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/wallet/script_row.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/script_proof.hpp"
#include "org/minima/objects/keys/signature.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/commands/backup/vault.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace send {

sendsign::sendsign()
    : org::minima::system::commands::Command("sendsign", "[file:] - Sign a previously created txn") {}

std::vector<std::string> sendsign::getValidParams() {
    return std::vector<std::string>{ "file", "password" };
}

std::string sendsign::getFullHelp() const {
    return
        "\nsendsign\n"
        "\n"
        "Sign a transaction previously created by the 'sendnosign' command, by specifying its .txn file.\n"
        "\n"
        "Optionally, if the node is Vault password locked, provide the Vault password to decrypt the keys for signing,\n"
        "\n"
        "the keys will be automatically re-encrypted after signing.\n"
        "\n"
        "Can be signed on an offline node, then posted from an online node.\n"
        "\n"
        "Outputs a new .txn file for the signed txn, to be posted with the 'sendpost' command.\n"
        "\n"
        "file:\n"
        "    Name of the unsigned transaction (.txn) file to sign, located in the node's base folder.\n"
        "    If not in the base folder, specify the full file path.\n"
        "\n"
        "password:\n"
        "    The Vault password, if the node is password locked.\n"
        "\n"
        "Examples:\n"
        "\n"
        "sendsign file:unsignedtransaction-1674907380057.txn\n"
        "\n"
        "sendsign file:C:\\Users\\unsignedtransaction-1674907380057.txn password:your_vaultpassword\n"
        "\n";
}

std::unique_ptr<org::minima::utils::json::JSONObject> sendsign::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::wallet::Wallet;
    // using org::minima::database::wallet::ScriptRow;
    using org::minima::objects::TxPoW;
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;
    using org::minima::objects::Coin;
    using org::minima::objects::ScriptProof;
    using org::minima::objects::keys::Signature;
    using org::minima::objects::base::MiniData;
    using org::minima::system::commands::backup::vault;
    using org::minima::system::commands::CommandException;
    using org::minima::utils::MiniFile;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    // Required filename param
    std::string filename = getParam("file");

    // Load the data
    std::filesystem::path file = MiniFile::createBaseFile(filename);
    std::vector<std::uint8_t> data = MiniFile::readCompleteFile(file);

    // Create MiniData
    MiniData sendtxpow(data);

    // Convert back into a TxPoW
    std::unique_ptr<TxPoW> txp = TxPoW::convertMiniDataVersion(sendtxpow);
    if (!txp) {
        throw CommandException("Failed to convert MiniData to TxPoW");
    }

    // Get the main Wallet
    Wallet& walletdb = MinimaDB::getDB()->getWallet();

    // Create a list of the required signatures
    std::vector<std::string> reqsigs;

    bool passwordlock = false;
    if (existsParam("password") && !MinimaDB::getDB()->getWallet().isBaseSeedAvailable()) {
        // Lets unlock the DB
        vault::passowrdUnlockDB(getParam("password"));
        // Lock at the end
        passwordlock = true;
    }

    // Get the sigs required.. for the main transaction
    Transaction& trans_main = txp->getTransaction();
    Witness& witness_main = txp->getWitness();
    auto& inputs_main = trans_main.getAllInputs();
    for (const std::shared_ptr<Coin>& input : inputs_main) {
        // Add the script proofs
        std::string scraddress = input->getAddress().to0xString();

        // Get the ScriptRow..
        std::unique_ptr<ScriptRow> srow = walletdb.getScriptFromAddress(scraddress);
        if (!srow) {
            throw CommandException(std::string("SERIOUS ERROR script missing for simple address : ") + scraddress);
        }
        // Script proof
        auto pscr = std::make_unique<ScriptProof>(srow->getScript());
        witness_main.addScript(std::move(pscr));

        // Add this address / public key to the list we need to sign as..
        std::string pubkey = srow->getPublicKey();
        if (std::find(reqsigs.begin(), reqsigs.end(), pubkey) == reqsigs.end()) {
            // Use the wallet..
            std::unique_ptr<Signature> signature = walletdb.signData(pubkey, trans_main.getTransactionID());
            // Add it..
            witness_main.addSignature(std::move(signature));
            // NOTE: Preserve Java behavior: do NOT add pubkey to reqsigs (Java code didn't), so duplicates are not suppressed.
        }
    }

    // Get the sigs required.. for the BURN transaction
    Transaction& trans_burn = txp->getBurnTransaction();
    Witness& witness_burn = txp->getBurnWitness();
    auto& inputs_burn = trans_burn.getAllInputs();
    for (const std::shared_ptr<Coin>& input : inputs_burn) {
        // Add the script proofs
        std::string scraddress = input->getAddress().to0xString();

        // Get the ScriptRow..
        std::unique_ptr<ScriptRow> srow = walletdb.getScriptFromAddress(scraddress);
        if (!srow) {
            throw CommandException(std::string("SERIOUS ERROR script missing for simple address : ") + scraddress);
        }
        auto pscr = std::make_unique<ScriptProof>(srow->getScript());
        witness_burn.addScript(std::move(pscr));

        // Add this address / public key to the list we need to sign as..
        std::string pubkey = srow->getPublicKey();
        if (std::find(reqsigs.begin(), reqsigs.end(), pubkey) == reqsigs.end()) {
            // Use the wallet..
            std::unique_ptr<Signature> signature = walletdb.signData(pubkey, trans_burn.getTransactionID());
            // Add it..
            witness_burn.addSignature(std::move(signature));
            // NOTE: Preserve Java behavior: do NOT add pubkey to reqsigs.
        }
    }

    // Are we locking the DB
    if (passwordlock) {
        // Lock the Wallet DB
        vault::passwordLockDB(getParam("password"));
    }

    // Calculate the TxPOWID
    txp->calculateTXPOWID();

    // Create the file name with current millis
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    std::string outname = std::string("signedtransaction-") + std::to_string(now_ms) + ".txn";
    std::filesystem::path txnfile = MiniFile::createBaseFile(outname);

    // Write it to a file..
    MiniFile::writeObjectToFile(txnfile, *txp);

    // Build response
    JSONObject sigtran;
    sigtran.put("txpow", std::filesystem::absolute(txnfile).string());

    ret->put("response", sigtran);

    return ret;
}

org::minima::system::commands::Command* sendsign::getFunction() {
    return new sendsign();
}

} // namespace send
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org