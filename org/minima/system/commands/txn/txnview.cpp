#include "org/minima/system/commands/txn/txnview.hpp"

#include <filesystem>
#include <memory>
#include <utility>

#include "org/minima/database/userprefs/txndb/txn_row.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txnview::txnview()
    : org::minima::system::commands::Command(
          "txnview",
          "(file:) (data:) - View a transaction as a JSON.") {}

std::string txnview::getFullHelp() const {
    return std::string("\ntxnimport\n")
         + "\n"
         + "View a transaction from previously exported HEX data or a .txn file.\n"
         + "\n"
         + "file: (optional)\n"
         + "    File name/path to the previously exported .txn file.\n"
         + "\n"
         + "data: (optional)\n"
         + "    HEX data of the previously exported transaction.\n"
         + "\n"
         + "Examples:\n"
         + "\n"
         + "txnview data:0x0000..\n"
         + "\n"
         + "txnview file:multisig.txn\n";
}

std::vector<std::string> txnview::getValidParams() const {
    return std::vector<std::string>{ "file", "data" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnview::runCommand() {
    using org::minima::database::userprefs::txndb::TxnRow;
    using org::minima::objects::base::MiniData;
    using org::minima::system::commands::CommandException;
    using org::minima::utils::MiniFile;
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    if (existsParam("file")) {
        const std::string file = getParam("file");
        std::filesystem::path ff = MiniFile::createBaseFile(file);

        if (!std::filesystem::exists(ff)) {
            throw CommandException(std::string("File does not exist : ")
                                   + std::filesystem::absolute(ff).string());
        }

        // Load it in
        std::vector<std::uint8_t> txndata = MiniFile::readCompleteFile(ff);

        // Convert to MiniData
        MiniData minitxn(txndata);

        // Convert this
        std::unique_ptr<TxnRow> txnrow = TxnRow::convertMiniDataVersion(minitxn);
        if (!txnrow) {
            throw CommandException("Invalid transaction data");
        }

        JSONObject resp; // Unused, mirrors Java structure
        ret->put("response", txnrow->toJSON());

    } else if (existsParam("data")) {
        // Get the HEX data
        std::unique_ptr<MiniData> dv = getDataParam("data");
        if (!dv) {
            throw CommandException("Invalid data parameter");
        }

        // Convert to a TxnRow
        std::unique_ptr<TxnRow> tx = TxnRow::convertMiniDataVersion(*dv);
        if (!tx) {
            throw CommandException("Invalid transaction data");
        }

        if (existsParam("id")) {
            tx->setID(getParam("id"));
        }

        JSONObject resp; // Unused, mirrors Java structure
        ret->put("response", tx->toJSON());

    } else {
        throw CommandException("Must specify file or data");
    }

    return ret;
}

org::minima::system::commands::Command* txnview::getFunction() {
    return new txnview();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org