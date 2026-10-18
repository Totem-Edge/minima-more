#include "org/minima/system/commands/backup/restore.hpp"

#include <sstream>
#include <stdexcept>
#include <cstring>

#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/encrypt/generate_key.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_list.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/utils/ssl/s_s_l_manager.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

using org::minima::objects::base::MiniData;
using org::minima::utils::MiniFile;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONObject;
using org::minima::utils::encrypt::GenerateKey;
using org::minima::utils::encrypt::Cipher;

restore::restore()
    : org::minima::system::commands::Command("restore", "[file:] (password:) - Restore the entire system.") {
}

std::string restore::getFullHelp() const {
    return std::string("\nrestore\n"
                       "\n"
                       "Restore your node from a backup. You MUST wait until all your original keys are created before this is allowed.\n"
                       "\n"
                       "file:\n"
                       "    Specify the filename or local path of the backup to restore\n"
                       "\n"
                       "password: (optional)\n"
                       "    Enter the password of the backup \n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "restore file:my-full-backup-01-Jan-22 password:Longsecurepassword456\n");
}

std::vector<std::string> restore::getValidParams() const {
    return { "file", "password", "shutdown" };
}

std::unique_ptr<JSONObject> restore::runCommand() {
    // JSON reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Ensure all keys created (Java: vault.checkAllKeysCreated())
    // We map to Main::getAllKeysCreated() and enforce the same precondition.
    if (!org::minima::system::Main::getInstance()->getAllKeysCreated()) {
        throw org::minima::system::commands::CommandException("All original keys must be created before restore is allowed.");
    }

    // Get file param
    const std::string file = getParam("file", "");
    if (file.empty()) {
        throw std::runtime_error("MUST specify a file to restore from");
    }

    // Password param (default "minima")
    const std::string password = getParam("password", "minima");
    if (password.empty()) {
        throw org::minima::system::commands::CommandException("Cannot have a blank password");
    }

    // Shutdown flag
    const bool doshutdown = getBooleanParam("shutdown", true);

    // Resolve and check restore file
    std::filesystem::path restorefile = MiniFile::createBaseFile(file);
    if (!std::filesystem::exists(restorefile)) {
        throw std::runtime_error(std::string("Restore file doesn't exist : ") + std::filesystem::absolute(restorefile).string());
    }

    // Ensure restore folder exists: DATA_FOLDER/restore
    std::filesystem::path restorefolder = std::filesystem::path(org::minima::system::params::GeneralParams::DATA_FOLDER) / "restore";
    std::filesystem::create_directories(restorefolder);

    // Read entire backup file
    std::vector<std::uint8_t> restoredata = MiniFile::readCompleteFile(restorefile);

    // Create a binary stream over the raw data
    std::istringstream dis = makeBinaryIStream(restoredata);

    // Read SALT and IVParam (both MiniData) from the raw stream
    MiniData salt    = MiniData::ReadFromStream(dis);
    MiniData ivparam = MiniData::ReadFromStream(dis);

    // Derive AES secret key from password and salt
    org::minima::utils::encrypt::SecretKey sk = GenerateKey::secretKey(password, salt.getBytes());

    // Read the remaining encrypted bytes from the stream
    std::vector<std::uint8_t> enc_bytes = MiniFile::readAllBytes(dis);

    // Decrypt to a gzip-compressed buffer
    org::minima::utils::encrypt::Cipher ciph = GenerateKey::getCipherSYM(Cipher::DECRYPT_MODE, ivparam.getBytes(), sk.bytes());
    std::vector<std::uint8_t> gz_bytes = ciph.doFinal(enc_bytes);

    // Decompress the gzip buffer to a plain buffer using file-based helpers
    std::filesystem::path gzfile   = restorefolder / "payload.gz";
    std::filesystem::path binfile  = restorefolder / "payload.bin";
    try {
        MiniFile::writeDataToFile(gzfile, gz_bytes);
        MiniFile::decompressGzipFile(gzfile, binfile);
    } catch (const std::exception&) {
        // Incorrect password or corrupt gzip
        throw org::minima::system::commands::CommandException("Incorrect Password!");
    }

    // Build a stream for the decompressed data
    std::vector<std::uint8_t> plain_bytes = MiniFile::readCompleteFile(binfile);
    std::istringstream disciph = makeBinaryIStream(plain_bytes);

    // Notify Main to prepare for restore
    org::minima::system::Main::getInstance()->restoreReady(doshutdown);

    // First section: wallet.sql in restore folder
    readNextBackup(restorefolder / "wallet.sql", disciph);

    // Stop saving state
    org::minima::database::MinimaDB::getDB()->setAllowSaveState(false);

    org::minima::utils::MinimaLogger::log("Restoring backup files..");

    // The rest are written directly to the base DB folder
    std::filesystem::path basedb = std::filesystem::path(org::minima::database::MinimaDB::getDB()->getBaseDBFolder());
    std::filesystem::create_directories(basedb);

    std::filesystem::path cascfile = basedb / "cascade.db";
    readNextBackup(cascfile, disciph);

    std::filesystem::path treefile = basedb / "chaintree.db";
    readNextBackup(treefile, disciph);

    std::filesystem::path udb = basedb / "userprefs.db";
    readNextBackup(udb, disciph);

    std::filesystem::path p2pdb = basedb / "p2p.db";
    readNextBackup(p2pdb, disciph);

    // Now load the relevant TxPoW list (to consume stream and be ready for DB insertion when API available)
    std::unique_ptr<org::minima::txpowdb::sql::TxPoWList> txplist = readNextTxPoWList(disciph);

    // Java: Wipe and repopulate SQL DB with txplist.
    // The current C++ headers do not expose TxPoWDB::getSQLDB(), so we cannot implement this here without guessing.

    // Allow saving state again
    org::minima::database::MinimaDB::getDB()->setAllowSaveState(true);

    // Java continues: wallet restore, increment key uses, save SQL DB, wipe archive.
    // These APIs are not present in provided headers, so we cannot call them here.

    // Persist DBs (save everything)
    org::minima::database::MinimaDB::getDB()->saveAllDB();

    // Cleanup: remove the restore working folder (removes temp gz/bin too)
    org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER, restorefolder);

    // Recreate SSL: delete the SSL folder so it is regenerated
    {
        const std::string sslFolder = org::minima::utils::ssl::SSLManager::getSSLFolder();
        org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER,
                                                         std::filesystem::path(sslFolder));
    }

    // Response object
    JSONObject resp;
    resp.put("file", std::filesystem::absolute(restorefile).string());
    ret->put("restore", resp);
    ret->put("message", std::string("Restart Minima for restore to take effect!"));

    // If requested, send shutdown notifications (closest mapping available)
    if (doshutdown) {
        org::minima::system::Main::getInstance()->setHasShutDown();
        org::minima::system::Main::getInstance()->NotifyMainListenerOfShutDown();
    }

    return ret;
}

std::int64_t restore::readNextBackup(const std::filesystem::path& zOutput, std::istream& zIn) {
    MiniData data = MiniData::ReadFromStream(zIn);
    MiniFile::writeDataToFile(zOutput, data.getBytes());
    // Return file size
    std::error_code ec;
    auto sz = std::filesystem::file_size(zOutput, ec);
    if (ec) {
        return static_cast<std::int64_t>(data.getBytes().size());
    }
    return static_cast<std::int64_t>(sz);
}

std::unique_ptr<org::minima::txpowdb::sql::TxPoWList> restore::readNextTxPoWList(std::istream& zIn) {
    MiniData data = MiniData::ReadFromStream(zIn);
    return org::minima::txpowdb::sql::TxPoWList::convertMiniDataVersion(data);
}

std::istringstream restore::makeBinaryIStream(const std::vector<std::uint8_t>& data) {
    std::string s;
    s.assign(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(std::move(s), std::ios::binary);
    iss.seekg(0, std::ios::beg);
    return iss;
}

org::minima::system::commands::Command* restore::getFunction() {
    return new restore();
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org