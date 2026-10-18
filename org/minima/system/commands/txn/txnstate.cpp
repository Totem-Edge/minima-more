#include "org/minima/system/commands/txn/txnstate.hpp"

#include <memory>
#include <utility>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txnstate::txnstate()
    : org::minima::system::commands::Command(
          "txnstate", "[id:] [port:] [value:] - Add a state variable") {
}

std::string txnstate::getFullHelp() const {
    return std::string("\ntxnstate\n"
                       "\n"
                       "Add a state variable to a transaction.\n"
                       "\n"
                       "id:\n"
                       "    The id of the transaction.\n"
                       "\n"
                       "port:\n"
                       "    Port number of the state variable, from 0-255.\n"
                       "\n"
                       "value:\n"
                       "    Value for the state variable.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "txnstate id:multisig port:0 value:0xFED5..\n"
                       "\n"
                       "txnstate id:multisig port:1 value:100 \n"
                       "\n"
                       "txnstate id:multisig port:1 value:\"string\" \n");
}

std::vector<std::string> txnstate::getValidParams() const {
    return std::vector<std::string>{ "id", "port", "value" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnstate::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::database::userprefs::txndb::TxnDB;
    using org::minima::database::userprefs::txndb::TxnRow;
    using org::minima::objects::Transaction;
    using org::minima::objects::StateVariable;
    using org::minima::system::commands::CommandException;

    // Prepare JSON reply
    auto ret = getJSONReply();

    // Access the custom transaction DB
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    // Parameters
    const std::string id    = getParam("id");
    const std::string portS = getParam("port");
    const std::string value = getParam("value");

    // Locate the transaction row
    TxnRow* txnrow = db.getTransactionRow(id);
    if (!txnrow) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }

    // Transaction reference
    Transaction& trans = txnrow->getTransaction();

    // Parse port and create state variable
    int port = 0;
    try {
        port = std::stoi(portS);
    } catch (const std::exception&) {
        // Match Java behavior where parseInt throws; propagate as CommandException
        throw CommandException(std::string("Invalid port value : ") + portS);
    }

    auto sv = std::make_unique<StateVariable>(port, value);

    // Add it to the transaction
    trans.addStateVariable(std::move(sv));

    // Recompute transaction ID
    trans.calculateTransactionID();

    // Return updated transaction JSON
    ret->put("response", db.getTransactionRow(id)->toJSON());

    return ret;
}

org::minima::system::commands::Command* txnstate::getFunction() {
    return new txnstate();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org