#include "org/minima/system/commands/txn/txnimport.hpp"

#include <filesystem>
#include <utility>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::utils::json::JSONObject;
using org::minima::system::commands::CommandException;

txnimport::txnimport()
    : org::minima::system::commands::Command(
          "txnimport",
          "(id:) (file:) (data:) - Import a transaction as a file or HEX data. Optionally specify the ID") {}

std::string txnimport::getFullHelp() const {
    return
        "\ntxnimport\n"
        "\n"
        "Import a transaction from previously exported HEX data or a .txn file.\n"
        "\n"
        "Optionally specify an ID to use for the new transaction.\n"
        "\n"
        "id: (optional)\n"
        "    Choose an ID for the transaction you are importing.\n"
        "\n"
        "file: (optional)\n"
        "    File name/path to the previously exported .txn file.\n"
        "\n"
        "data: (optional)\n"
        "    HEX data of the previously exported transaction.\n"
        "\n"
        "Examples:\n"
        "\n"
        "txnimport data:0x0000..\n"
        "\n"
        "txnimport id:simpletxn data:0x0000..\n"
        "\n"
        "txnimport id:multisig file:multisig.txn\n";
}

std::vector<std::string> txnimport::getValidParams() const {
    return { "id", "file", "data" };
}

std::unique_ptr<JSONObject> txnimport::runCommand() {
    auto ret = getJSONReply();

    // Access the custom transaction DB
    org::minima::database::userprefs::txndb::TxnDB& db =
        org::minima::database::MinimaDB::getDB()->getCustomTxnDB();

    if (existsParam("file")) {
        // Resolve and validate the file
        std::string file = getParam("file");
        std::filesystem::path ff = org::minima::utils::MiniFile::createBaseFile(file);
        if (!std::filesystem::exists(ff)) {
            throw CommandException("File does not exist : " + std::filesystem::absolute(ff).string());
        }

        // Load file bytes
        std::vector<std::uint8_t> txndata = org::minima::utils::MiniFile::readCompleteFile(ff);

        // Convert bytes to MiniData
        org::minima::objects::base::MiniData minitxn(txndata);

        // Convert MiniData to TxnRow
        auto txnrow = org::minima::database::userprefs::txndb::TxnRow::convertMiniDataVersion(minitxn);
        if (!txnrow) {
            throw CommandException("Invalid transaction data in file");
        }

        if (existsParam("id")) {
            txnrow->setID(getParam("id"));
        }

        // Prepare response JSON before moving txnrow into the DB
        JSONObject respjson = txnrow->toJSON();

        // Add to DB
        db.addCompleteTransaction(std::move(txnrow));

        // Attach response
        ret->put("response", respjson);

    } else if (existsParam("data")) {
        // Get the HEX data
        auto dv_ptr = getDataParam("data");
        if (!dv_ptr) {
            throw CommandException("Invalid or missing data parameter");
        }
        const org::minima::objects::base::MiniData& dv = *dv_ptr;

        // Convert to a TxnRow
        auto tx = org::minima::database::userprefs::txndb::TxnRow::convertMiniDataVersion(dv);
        if (!tx) {
            throw CommandException("Invalid transaction data");
        }

        if (existsParam("id")) {
            tx->setID(getParam("id"));
        }

        // Prepare response JSON before moving tx into the DB
        JSONObject respjson = tx->toJSON();

        // Add to the DB
        db.addCompleteTransaction(std::move(tx));

        // Attach response
        ret->put("response", respjson);

    } else {
        throw CommandException("Must specify file or data");
    }

    return ret;
}

org::minima::system::commands::Command* txnimport::getFunction() {
    return new txnimport();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org