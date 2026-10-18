#include "org/minima/system/commands/txn/txnoutput.hpp"

#include <memory>
#include <string>
#include <vector>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/token.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::txndb::TxnDB;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::objects::Coin;
using org::minima::objects::Token;
using org::minima::objects::Transaction;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::brains::TxPoWGenerator;
using org::minima::system::brains::TxPoWSearcher;
using org::minima::system::commands::CommandException;
using org::minima::utils::json::JSONObject;

txnoutput::txnoutput()
    : org::minima::system::commands::Command(
          "txnoutput",
          "[id:] [amount:] [address:] (tokenid:) (storestate:) - Create a transaction output") {}

std::string txnoutput::getFullHelp() const {
    return
        "\ntxnoutput\n"
        "\n"
        "Create a transaction output.\n"
        "\n"
        "This will create a new coin (UTxO).\n"
        "\n"
        "If the sum of inputs > outputs, the difference will be burned unless change to the sender is defined as an output.\n"
        "\n"
        "Optionally store the transaction state variables in the new output coin.\n"
        "\n"
        "id:\n"
        "    The id of the transaction to add an output to.\n"
        "\n"
        "amount:\n"
        "    The amount for the output. To send to the specified address.\n"
        "\n"
        "address:\n"
        "    Address of the recipient/script to send the output to. Can be 0x or Mx address.\n"
        "\n"
        "tokenid: (optional)\n"
        "    tokenid of the output. Default is Minima (0x00).\n"
        "\n"
        "storestate: (optional)\n"
        "    true or false, true will keep the state variables of the transaction in the newly created output coin.\n"
        "    Default is true.\n"
        "\n"
        "Examples:\n"
        "\n"
        "txnoutput id:simpletxn amount:10 address:0xFED5..\n"
        "\n"
        "txnoutput id:multisig amount:10 address:0xFED5.. tokenid:0xCEF5.. storestate:false\n"
        "\n"
        "txnoutput id:eltootxn amount:10 address:0xFED5..\n";
}

std::vector<std::string> txnoutput::getValidParams() const {
    return {"id","amount","address","tokenid","storestate"};
}

std::unique_ptr<JSONObject> txnoutput::runCommand() {
    auto ret = getJSONReply();

    // Access custom transaction DB
    TxnDB* db = &MinimaDB::getDB()->getCustomTxnDB();

    // Required params
    std::string id = getParam("id");
    std::unique_ptr<MiniNumber> amount_up = getNumberParam("amount");
    std::string addr_norm = getAddressParam("address");
    MiniData address(addr_norm);
    bool storestate = getBooleanParam("storestate", true);

    // Token handling (default Minima)
    MiniData tokenid = Token::TOKENID_MINIMA;
    std::shared_ptr<Token> token_sp;

    if (existsParam("tokenid")) {
        std::unique_ptr<MiniData> tokenid_up = getDataParam("tokenid");
        tokenid = *tokenid_up;

        if (!tokenid.isEqual(Token::TOKENID_MINIMA)) {
            token_sp = TxPoWSearcher::getToken(tokenid);
            if (!token_sp) {
                throw CommandException(std::string("Token not found : ") + tokenid.toString());
            }
        }
    }

    // Actual amount in Minima (scale if token specified)
    MiniNumber miniamount = *amount_up;
    if (token_sp) {
        std::unique_ptr<MiniNumber> scaled = token_sp->getScaledMinimaAmount(*amount_up);
        miniamount = *scaled;
    }

    // Create the coin (output)
    std::unique_ptr<Coin> output = std::make_unique<Coin>(
        Coin::COINID_OUTPUT, address, miniamount, tokenid, storestate);

    if (token_sp) {
        // Deep-copy the token via Streamable serialization round-trip
        std::unique_ptr<MiniData> tokmd = MiniData::getMiniDataVersion(*token_sp);
        if (!tokmd) {
            throw CommandException("Failed to serialize token for output embedding");
        }
        std::unique_ptr<Token> tokcopy = Token::convertMiniDataVersion(*tokmd);
        if (!tokcopy) {
            throw CommandException("Failed to deserialize token for output embedding");
        }
        output->setToken(std::move(tokcopy));
    }

    // Retrieve the transaction row
    TxnRow* txnrow = db->getTransactionRow(id);
    if (!txnrow) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }

    Transaction& trans = txnrow->getTransaction();
    trans.addOutput(std::move(output));

    // Precompute CoinIDs and calculate transaction ID
    TxPoWGenerator::precomputeTransactionCoinID(trans);
    trans.calculateTransactionID();

    // Return current transaction JSON
    ret->put("response", db->getTransactionRow(id)->toJSON());

    return ret;
}

org::minima::system::commands::Command* txnoutput::getFunction() {
    return new txnoutput();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org