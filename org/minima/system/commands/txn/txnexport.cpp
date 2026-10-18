#include "org/minima/system/commands/txn/txnexport.hpp"

#include <filesystem>
#include <system_error>
#include <cstdint>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_d_b.hpp"
#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

using org::minima::database::MinimaDB;
using org::minima::database::userprefs::txndb::TxnDB;
using org::minima::database::userprefs::txndb::TxnRow;
using org::minima::objects::base::MiniData;
using org::minima::system::commands::CommandException;
using org::minima::utils::MiniFile;
using org::minima::utils::MiniFormat;
using org::minima::utils::json::JSONObject;

txnexport::txnexport()
    : org::minima::system::commands::Command(
          "txnexport",
          "[id:] (file:) - Export a transaction as HEX or to a file") {}

std::string txnexport::getFullHelp() const {
    return std::string()
        + "\ntxnexport\n"
        + "\n"
        + "Export a transaction as HEX or to a file.\n"
        + "\n"
        + "The output can then be imported using 'txnimport' to another node. E.g. For signing.\n"
        + "\n"
        + "id:\n"
        + "    The id of the transaction to export.\n"
        + "\n"
        + "file: (optional)\n"
        + "    File name/path to export the transaction to, must use the .txn extension.\n"
        + "\n"
        + "Examples:\n"
        + "\n"
        + "txnexport id:simpletxn\n"
        + "\n"
        + "txnexport id:multisig file:multisig.txn\n";
}

std::vector<std::string> txnexport::getValidParams() const {
    return {"id", "file", "showtxn"};
}

std::unique_ptr<JSONObject> txnexport::runCommand() {
    auto ret = getJSONReply();

    // Access the custom transaction DB
    TxnDB& db = MinimaDB::getDB()->getCustomTxnDB();

    // Required ID parameter
    std::string id = getParam("id");

    // Optional showtxn flag
    bool showtxn = getBooleanParam("showtxn", false);

    // Get the transaction row
    TxnRow* txnrow = db.getTransactionRow(id);
    if (txnrow == nullptr) {
        throw CommandException(std::string("Transaction not found : ") + id);
    }

    if (existsParam("file")) {
        // File export path
        std::string file = getParam("file");

        // Create the file path
        std::filesystem::path output = MiniFile::createBaseFile(file);

        // If it exists, delete it (mirror Java's behavior; ignore failure)
        std::error_code ec;
        if (std::filesystem::exists(output, ec)) {
            std::filesystem::remove(output, ec); // ignore errors to match Java's delete()
        }

        // Export txnrow to the file
        MiniFile::writeObjectToFile(output, *txnrow);

        // Prepare response
        JSONObject resp;
        std::filesystem::path abs = std::filesystem::absolute(output, ec);
        resp.put("file", abs.string());

        std::uintmax_t fsize = std::filesystem::file_size(output, ec);
        long long lsize = ec ? 0LL : static_cast<long long>(fsize);
        resp.put("size", MiniFormat::formatSize(lsize));

        ret->put("response", resp);
    } else {
        // Output to HEX
        std::unique_ptr<MiniData> dv = MiniData::getMiniDataVersion(*txnrow);

        JSONObject resp;
        if (showtxn) {
            resp.put("txn", txnrow->toJSON());
        }

        // dv should not be null for valid Streamable
        std::string hex = dv ? dv->to0xString() : std::string();
        resp.put("data", hex);

        ret->put("response", resp);
    }

    return ret;
}

org::minima::system::commands::Command* txnexport::getFunction() {
    return new txnexport();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org