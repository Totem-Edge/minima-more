#include "org/minima/system/commands/backup/restoresync.hpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_list.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"

#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/commands/network/connect.hpp"

#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/encrypt/generate_key.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/ssl/s_s_l_manager.hpp"

namespace fs = std::filesystem;

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

using org::minima::utils::json::JSONObject;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

restoresync::restoresync()
    : Command("restoresync", "[file:] (password:) (host:) (keyuses:) - Restore the entire system AND perform an archive sync. Use when the backup is old.") {}

std::string restoresync::getFullHelp() const {
    return std::string("\nrestoresync\n"
        "\n"
        "Restore your node from a backup and then sync to the top block using an archive node.\n"
        "\n"
        "You MUST wait until all the keys for the node are created before this is allowed.\n"
        "\n"
        "Reverts to a standard restore if backup is not older than 2 days.\n"
        "\n"
        "file:\n"
        "    Specify the filename or local path of the backup to restore\n"
        "\n"
        "password: (optional)\n"
        "    Enter the password of the backup \n"
        "\n"
        "host: (optional)\n"
        "    ip:port of the archive node to sync from.\n"
        "\n"
        "keyuses: (optional) \n"
        "    Increment (not set) the number of key uses per key.\n"
        "\n"
        "Examples:\n"
        "\n"
        "restoresync file:my-full-backup-01-Jan-22 password:Longsecurepassword456 host:89.98.89.98:9001\n");
}

std::vector<std::string> restoresync::getValidParams() const {
    return std::vector<std::string>{ "file", "password", "host" };
}

std::uintmax_t restoresync::readNextBackup(const fs::path& zOutput, std::istream& zIn) {
    MiniData data = MiniData::ReadFromStream(zIn);
    org::minima::utils::MiniFile::writeDataToFile(zOutput, data.getBytes());
    std::error_code ec;
    auto sz = fs::file_size(zOutput, ec);
    return ec ? 0 : sz;
}

std::unique_ptr<org::minima::txpowdb::sql::TxPoWList>
restoresync::readNextTxPoWList(std::istream& zIn) {
    MiniData data = MiniData::ReadFromStream(zIn);
    return org::minima::txpowdb::sql::TxPoWList::convertMiniDataVersion(data);
}

fs::path restoresync::decryptAndDecompressToTemp(const std::vector<std::uint8_t>& zRestoreData,
                                                 const fs::path& zRestoreFolder,
                                                 const std::string& zPassword) {
    // Wrap input bytes in stream
    std::string sdata(reinterpret_cast<const char*>(zRestoreData.data()), zRestoreData.size());
    std::istringstream in(sdata, std::ios::binary);

    // Read SALT and IV MiniData (cleartext)
    MiniData salt = MiniData::ReadFromStream(in);
    MiniData ivparam = MiniData::ReadFromStream(in);

    // Read remaining ciphertext
    std::vector<std::uint8_t> cipherbytes = org::minima::utils::MiniFile::readAllBytes(in);

    // Derive secret
    std::vector<std::uint8_t> secret = org::minima::utils::encrypt::GenerateKey::secretKey(zPassword, salt.getBytes()).bytes();

    // AES decrypt
    auto cipher = org::minima::utils::encrypt::GenerateKey::getCipherSYM(
        org::minima::utils::encrypt::Cipher::DECRYPT_MODE,
        ivparam.getBytes(),
        secret
    );

    std::vector<std::uint8_t> decrypted;
    try {
        decrypted = cipher.doFinal(cipherbytes);
    } catch (const std::exception&) {
        throw org::minima::system::commands::CommandException("Incorrect Password!");
    }

    // Write decrypted gzip bytes to temp .gz file
    fs::path gzfile = zRestoreFolder / "restoresync_decrypted.gz";
    fs::path decomp = zRestoreFolder / "restoresync_decompressed.dat";
    org::minima::utils::MiniFile::writeDataToFile(gzfile, decrypted);

    // Decompress gzip -> decompressed file
    try {
        org::minima::utils::MiniFile::decompressGzipFile(gzfile, decomp);
    } catch (const std::exception&) {
        // Incorrect password or corrupted data manifests here
        throw org::minima::system::commands::CommandException("Incorrect Password!");
    }

    // Cleanup gz file
    std::error_code ec;
    fs::remove(gzfile, ec);

    return decomp;
}

std::unique_ptr<org::minima::utils::json::JSONObject> restoresync::runCommand() {
    auto ret = getJSONReply();

    // All keys must be created first
    if (!org::minima::system::Main::getInstance()->getAllKeysCreated()) {
        throw org::minima::system::commands::CommandException("All keys must be created before restore is allowed");
    }

    // Params
    std::string file = getParam("file", "");
    if (file.empty()) {
        throw std::runtime_error("MUST specify a file to restore from");
    }

    std::string password = getParam("password", "minima");
    if (password.empty()) {
        throw org::minima::system::commands::CommandException("Cannot have a blank password");
    }

    // Does file exist?
    fs::path restorefile = org::minima::utils::MiniFile::createBaseFile(file);
    if (!fs::exists(restorefile)) {
        throw std::runtime_error(std::string("Restore file doesn't exist : ") + fs::absolute(restorefile).string());
    }

    // Base restore folder
    fs::path restorefolder = fs::path(org::minima::system::params::GeneralParams::DATA_FOLDER) / "restore";
    std::error_code ec_mk;
    fs::create_directories(restorefolder, ec_mk);

    // Read file bytes
    std::vector<std::uint8_t> restoredata = org::minima::utils::MiniFile::readCompleteFile(restorefile);

    // Decrypt and decompress to temp file; will throw CommandException on failure
    fs::path decompfile = decryptAndDecompressToTemp(restoredata, restorefolder, password);

    // If it has not stopped - First stop everything.. and get ready to restore the files..
    org::minima::system::Main::getInstance()->restoreReady(false);

    // Open the decompressed data stream
    std::ifstream disciph(decompfile, std::ios::binary);
    if (!disciph) {
        throw std::runtime_error("Failed to open decompressed restore data");
    }

    // Read wallet.sql into restorefolder
    fs::path walletsql = restorefolder / "wallet.sql";
    readNextBackup(walletsql, disciph);

    // Stop saving state during critical file restore
    org::minima::database::MinimaDB::getDB()->setAllowSaveState(false);

    org::minima::utils::MinimaLogger::log("Restoring backup files..");

    // Base DB folder
    fs::path basedb = org::minima::database::MinimaDB::getDB()->getBaseDBFolder();

    fs::path cascfile = basedb / "cascade.db";
    readNextBackup(cascfile, disciph);

    fs::path treefile = basedb / "chaintree.db";
    readNextBackup(treefile, disciph);

    fs::path udb = basedb / "userprefs.db";
    readNextBackup(udb, disciph);

    fs::path p2pdb = basedb / "p2p.db";
    readNextBackup(p2pdb, disciph);

    // Now load the relevant TxPoW list
    auto txplist = readNextTxPoWList(disciph);

    // Re-enable saving state before DB reload
    org::minima::database::MinimaDB::getDB()->setAllowSaveState(true);

    // Reload DBs relevant for restore sync
    org::minima::database::MinimaDB::getDB()->loadDBsForRestoreSync();

    // Add TxPoWs into SQL DB using a local TxPoWSqlDB instance
    org::minima::database::txpowdb::sql::TxPoWSqlDB txpsqldb;
    txpsqldb.wipeDB();
    for (auto& txp : txplist->mTxPoWs) {
        if (txp) {
            txpsqldb.addTxPoW(*txp, true);
        }
    }
    txpsqldb.closeAndReopen(); // ensure changes are flushed

    // And now clean up..
    org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER, restorefolder);

    // Recreate the SSL keystore (delete SSL folder so it will be regenerated)
    {
        fs::path sslfolder(org::minima::utils::ssl::SSLManager::getSSLFolder());
        org::minima::utils::MiniFile::deleteFileOrFolder(org::minima::system::params::GeneralParams::DATA_FOLDER, sslfolder);
    }

    // Reopen the required SQL Dbs..
    org::minima::system::Main::getInstance()->restoreReadyForSync();

    // Shall we do a sync? Compare latest TxPoW time
    long long timemilli = 0;
    {
        auto latest = txpsqldb.getLatestTxPoW(1, 0);
        if (!latest.empty() && latest[0]) {
            timemilli = latest[0]->getTimeMilli().getAsLong();
        } else {
            // If none found, treat as very old backup to force resync when host is provided
            timemilli = 0;
        }
    }
    long long timediff = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count()) - timemilli;
    long long maxtime = 1000LL * 60LL * 60LL * 24LL * 2LL;

    // keyuses default 256
    int keyuses = 256;
    try {
        auto keyusesmn = getNumberParam("keyuses", MiniNumber(256));
        if (keyusesmn) keyuses = keyusesmn->getAsInt();
    } catch (...) {
        keyuses = 256;
    }

    if (timediff < maxtime || !existsParam("host")) {
        if (existsParam("host")) {
            org::minima::utils::MinimaLogger::log("No Sync required as new backup");
        }

        // Update key uses (increment)
        org::minima::database::MinimaDB::getDB()->getWallet().updateIncrementAllKeyUses(keyuses);

        // Don't do the usual shutdown hook
        org::minima::system::Main::getInstance()->setHasShutDown();

        // Save databases
        org::minima::database::MinimaDB::getDB()->saveAllDB();

        // Shutdown
        org::minima::system::Main::getInstance()->shutdown(false);

        // Tell listener
        org::minima::system::Main::getInstance()->NotifyMainListenerOfShutDown();

        // Response
        auto resp2 = std::make_unique<JSONObject>();
        resp2->put("file", fs::absolute(restorefile).string());
        ret->put("restore", *resp2);
        ret->put("message", "Restart Minima for restore to take effect!");

        return ret;
    }

    // Resync if host provided
    if (existsParam("host")) {
        // Determine start block as (latest - 128), floored at 0
        MiniNumber startblock = MiniNumber(0);
        {
            auto latest = txpsqldb.getLatestTxPoW(1, 0);
            if (!latest.empty() && latest[0]) {
                MiniNumber latestBlock = latest[0]->getBlockNumber();
                MiniNumber oneTwoEight(128);
                if (latestBlock.isMore(oneTwoEight)) {
                    startblock = latestBlock.sub(oneTwoEight);
                } else {
                    startblock = MiniNumber::ZERO();
                }
            }
        }

        org::minima::utils::MinimaLogger::log(std::string("Start sync from ") + startblock.toString());

        std::string host = getParam("host");

        // Perform resync (stubbed due to missing archive networking API)
        auto res = performResync(host, keyuses, startblock, true);

        // Log end sync based on latest txpow
        auto latestAfter = txpsqldb.getLatestTxPoW(1, 0);
        if (!latestAfter.empty() && latestAfter[0]) {
            org::minima::utils::MinimaLogger::log(std::string("End sync on ") + latestAfter[0]->getBlockNumber().toString());
        }
    }

    // Don't do the usual shutdown hook
    org::minima::system::Main::getInstance()->setHasShutDown();

    // Save all and shutdown
    org::minima::database::MinimaDB::getDB()->saveAllDB();
    org::minima::system::Main::getInstance()->shutdown(false);

    // Notify
    org::minima::system::Main::getInstance()->NotifyMainListenerOfShutDown();

    // Response
    auto resp = std::make_unique<JSONObject>();
    resp->put("file", fs::absolute(restorefile).string());
    ret->put("restore", *resp);
    ret->put("message", "Restart Minima for restore to take effect!");

    return ret;
}

org::minima::system::commands::Command* restoresync::getFunction() {
    return new restoresync();
}

std::unique_ptr<org::minima::utils::json::JSONObject>
restoresync::performResync(const std::string& zHost, int zKeyUses,
                           const org::minima::objects::base::MiniNumber& zStartBlock,
                           bool zIncrementKeys) {
    using org::minima::utils::MinimaLogger;

    // Update wallet key uses as per input
    auto& wallet = org::minima::database::MinimaDB::getDB()->getWallet();
    if (zIncrementKeys) {
        wallet.updateIncrementAllKeyUses(zKeyUses);
    } else {
        wallet.updateAllKeyUses(zKeyUses);
    }

    // Parse host:port (validates format)
    auto msg = org::minima::system::commands::network::connect::createConnectMessage(zHost);
    if (!msg) {
        throw org::minima::system::commands::CommandException("Invalid host format");
    }
    std::string host = msg->getString("host");
    int port = msg->getInteger("port");

    // Reset default data before resync
    org::minima::system::Main::getInstance()->archiveResetReady(false, false);

    // Notify (placeholder - archive networking module not present)
    MinimaLogger::log(std::string("Loading sync blocks from ") + zStartBlock.toString());

    auto resp = std::make_unique<JSONObject>();
    resp->put("message", "Archive sync requires archive networking module; not available in this build context.");
    resp->put("start", zStartBlock.toString());
    resp->put("end", zStartBlock.toString());
    resp->put("host", host + ":" + std::to_string(port));
    return resp;
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org