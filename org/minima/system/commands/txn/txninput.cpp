#include "org/minima/system/commands/txn/txninput.hpp"

#include <memory>
#include <utility>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include "org/minima/system/commands/txn/txnutils.hpp"

#ifdef _WIN32
// Windows-specific includes if needed in future
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::txndb::TxnDB;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::objects::Coin;
using org::minima::objects::Transaction;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::brains::TxPoWGenerator;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::system::commands::CommandException;
using org::minima::utils::json::JSONObject;

txninput::txninput()
    : org::minima::system::commands::Command(
          "txninput",
          "[id:] (coinid:) (coindata:) (floating:) (address:) (amount:) (tokenid:) (scriptmmr:true)- Add a coin as an input to a transaction") {}

std::string txninput::getFullHelp() const {
    return std::string()
        + "\ntxninput\n"
        + "\n"
        + "Add a coin as an input to a transaction.\n"
        + "\n"
        + "Optionally specify address, amount and tokenid to use an unspecified coinid - a floating ELTOO coin.\n"
        + "\n"
        + "A floating coin can be attached to multiple different existing coins as long as it has the same\n"
        + "\n"
        + "address, amount and tokenid - but different coinid.\n"
        + "\n"
        + "id:\n"
        + "    The id of the transaction to add an input to.\n"
        + "\n"
        + "coinid: (optional)\n"
        + "    The id of the coin to add as an input.\n"
        + "\n"
        + "coindata: (optional)\n"
        + "    The data of the coin to add, instead of coinid.\n"
        + "    Can be from the 'coinexport' command or 'outputcoindata' from another transaction.\n"
        + "\n"
        + "floating: (optional)\n"
        + "    true or false, true will add an unspecified, floating ELTOO coin as an input.\n"
        + "    If true, also specify address, amount, tokenid.\n"
        + "    If false, specify a coinid or coindata.\n"
        + "\n"
        + "address: (optional)\n"
        + "    Coin address to use for the floating input. Can be 0x or Mx address.\n"
        + "    The coin that is used \n"
        + "\n"
        + "amount: (optional)\n"
        + "    Amount of a coin for the floating input.\n"
        + "\n"
        + "tokenid: (optional)\n"
        + "    tokenid of a coin for the floating input.\n"
        + "\n"
        + "scriptmmr: (optional)\n"
        + "    true or false, true will add the scripts and MMR proof for the coin.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "txninput id:simpletxn coinid:0xD0BF..\n"
        + "\n"
        + "txninput id:multisig coinid:0xD0BF.. scriptmmr:true\n"
        + "\n"
        + "txninput id:posttxn coindata:0x000..\n"
        + "\n"
        + "txninput id:eltootxn floating:true address:0xFED5.. amount:10 tokenid:0x00\n";
}

std::vector<std::string> txninput::getValidParams() {
    return std::vector<std::string>{
        "id","coinid","coindata","floating","address","amount","tokenid","scriptmmr"
    };
}

std::unique_ptr<JSONObject> txninput::runCommand() {
    auto ret = getJSONReply();

    // The transaction ID
    const std::string id = getParam("id");

    // DB and row lookup
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();
    TxnRow* txnrow = db.getTransactionRow(id);
    if (txnrow == nullptr) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }

    // Floating input?
    const bool eltoo = getBooleanParam("floating", false);

    // Construct the Coin (unique_ptr)
    std::unique_ptr<Coin> cc;

    if (existsParam("coinid")) {
        const std::string coinid = getParam("coinid");

        // Search for the coin on-chain/in-db
        std::shared_ptr<Coin> found = TxPoWSearcher::searchCoin(MiniData(coinid));
        if (!found) {
            throw CommandException(std::string("CoinID not found : ") + coinid);
        }

        // Work on a deep copy for local transaction usage
        cc = found->deepCopy();

        // Floating coin?
        if (eltoo) {
            cc->resetCoinID(Coin::COINID_ELTOO);
        }

    } else if (existsParam("coindata")) {
        const std::string coindata = getParam("coindata");

        // Import coin from MiniData blob
        cc = Coin::convertMiniDataVersion(MiniData(coindata));
        if (!cc) {
            throw CommandException("ERROR importing coin data");
        }

        // Floating coin?
        if (eltoo) {
            cc->resetCoinID(Coin::COINID_ELTOO);
        }

    } else {
        // Create a floating ELTOO coin from address/amount/tokenid
        const std::string address = getAddressParam("address");
        const std::string amount  = getParam("amount");
        const std::string tokenid = getParam("tokenid", "0x00");

        cc = std::make_unique<Coin>(
            Coin::COINID_ELTOO,
            MiniData(address),
            MiniNumber(amount),
            MiniData(tokenid)
        );
    }

    // Add input to the transaction
    Transaction& trans = txnrow->getTransaction();

    // Keep a raw pointer in case scriptmmr block needs it after move
    Coin* cc_raw = cc.get();
    trans.addInput(std::move(cc));

    // Precompute output CoinIDs if possible
    TxPoWGenerator::precomputeTransactionCoinID(trans);

    // Calculate transaction ID
    trans.calculateTransactionID();

    // Optionally add scripts and MMR for this coin
    const bool smmr = getBooleanParam("scriptmmr", false);
    if (smmr && cc_raw != nullptr) {
        // Add details for this coin (implementation provided by txnutils if available)
        txnutils::setMMRandScripts(*cc_raw, txnrow->getWitness());
    }

    // Output the current transaction row as response
    TxnRow* outrow = db.getTransactionRow(id);
    if (outrow != nullptr) {
        ret->put("response", outrow->toJSON());
    } else {
        // Should not happen if earlier checks passed, but be safe
        throw CommandException(std::string("Transaction not found after update: ") + id);
    }

    return ret;
}

org::minima::system::commands::Command* txninput::getFunction() {
    return new txninput();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
