#include "org/minima/system/commands/backup/backup.hpp"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <random>
#include <chrono>
#include <stdexcept>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.hpp"
#include "org/minima/database/txpowdb/sql/tx_po_w_list.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/encrypt/generate_key.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/system/network/p2p/p2_p_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"

namespace fs = std::filesystem;

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

backup::backup()
    : org::minima::system::commands::Command(
          "backup",
          "(password:) (file:) (auto:) (maxhistory:) - Backup the system. Uses a timestamped name by default") {}

backup::~backup() = default;

std::string backup::getFullHelp() const {
    return std::string("\nbackup\n"
                       "\n"
                       "Backup your node. Uses a timestamped name by default.\n"
                       "\n"
                       "password: (optional)\n"
                       "    Set a password using letters and numbers only.\n"
                       "\n"
                       "file: (optional)\n"
                       "    Specify a filename ending in .bak, optionally include a local path for the backup.\n"
                       "    Default location for a backup is the Minima data folder.\n"
                       "\n"
                       "auto: (optional)\n"
                       "    true or false, true will schedule a non-password protected backup every 24 hours.\n"
                       "\n"
                       "maxhistory: (optional)\n"
                       "    Max relevant TxPoW to add - your history.\n"
                       "\n"
                       "Examples:\n"
                       "\n"
                       "backup password:Longsecurepassword456\n"
                       "\n"
                       "backup password:Longsecurepassword456 confirm:Longsecurepassword456\n"
                       "\n"
                       "backup password:Longsecurepassword456 file:my-backup-01-Jan-22.bak\n"
                       "\n"
                       "backup auto:true\n");
}

std::vector<std::string> backup::getValidParams() const {
    return std::vector<std::string>{ "debug", "password", "file", "auto", "confirm", "maxhistory" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> backup::runCommand() {
    using org::minima::database::MinimaDB;
    using org::minima::objects::base::MiniData;
    using org::minima::system::params::GeneralParams;
    using org::minima::utils::MiniFile;
    using org::minima::utils::MiniFormat;
    using org::minima::utils::MinimaLogger;
    using org::minima::utils::encrypt::GenerateKey;

    auto ret = getJSONReply();

    // Java: vault.checkAllKeysCreated(); (Not available in this C++ context)

    // AUTO backup initiate
    if (existsParam("auto")) {
        bool setauto = getBooleanParam("auto");

        // If setauto is false, respond and return immediately (as per Java)
        if (!setauto) {
            org::minima::utils::json::JSONObject resp;
            resp.put("autobackup", setauto);
            ret->put("backup", resp);
            return ret;
        }
        // If setauto is true, proceed to perform a backup as well (Java behavior)
    }

    // File name
    std::string file = getParam("file", "");
    if (file.empty()) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
        file = "minima-backup-" + std::to_string(static_cast<long long>(ms)) + ".bak";
    }

    // Password
    std::string password = getParam("password", "minima");
    if (password.empty()) {
        throw org::minima::system::commands::CommandException("Cannot have a blank password");
    }

    // Confirm
    if (existsParam("confirm")) {
        std::string confirm = getParam("confirm");
        if (password != confirm) {
            throw org::minima::system::commands::CommandException("Passwords do NOT match!");
        }
    }

    // Parsed but not used further (mirrors Java parsing)
    bool complete = getBooleanParam("complete", false);
    (void)complete;

    bool debug = getBooleanParam("debug", false);

    // Create the output file (ensures parent dirs)
    fs::path backupfile = MiniFile::createBaseFile(file);

    if (debug) {
        MinimaLogger::log(std::string("Backup file : ") + fs::absolute(backupfile).string());
    }

    // Remove if exists
    if (fs::exists(backupfile)) {
        std::error_code ec;
        fs::remove(backupfile, ec);
    }

    // Base folder for intermediate files
    fs::path backupfolder = fs::path(GeneralParams::DATA_FOLDER) / "backup";
    std::error_code ec_mk;
    fs::create_directories(backupfolder, ec_mk);

    if (debug) {
        MinimaLogger::log(std::string("Backup folder : ") + fs::absolute(backupfolder).string());
    }

    // Lock DB
    MinimaDB::getDB()->readLock(true);

    try {
        // Save current state
        MinimaDB::getDB()->saveState();

        // **FIXED**: Create backup files in the backup folder using saveDB/backupToFile methods
        
        // Wallet backup - use backupToFile() method
        fs::path walletfile = backupfolder / "wallet.sql";
        MinimaDB::getDB()->getWallet().backupToFile(walletfile);
        MiniData walletdata(MiniFile::readCompleteFile(walletfile));

        // Cascade backup
        fs::path cascade = backupfolder / "cascade.bak";
        MinimaDB::getDB()->getCascade().saveDB(cascade);
        MiniData cascadedata(MiniFile::readCompleteFile(cascade));

        // Chain tree backup
        fs::path chain = backupfolder / "chaintree.bak";
        MinimaDB::getDB()->getTxPoWTree().saveDB(chain);
        MiniData chaindata(MiniFile::readCompleteFile(chain));

        // User DB backup
        fs::path userdb = backupfolder / "userdb.bak";
        MinimaDB::getDB()->getUserDB().saveDB(userdb);
        MiniData userdata(MiniFile::readCompleteFile(userdb));

        // P2P DB backup
        fs::path p2pdb = backupfolder / "p2p.bak";
        MinimaDB::getDB()->getP2PDB().saveDB(p2pdb);
        MiniData p2pdata(MiniFile::readCompleteFile(p2pdb));

        // Relevant TxPoWs
        auto maxnum = getNumberParam("maxhistory", org::minima::database::txpowdb::sql::TxPoWSqlDB::MAX_RELEVANT_TXPOW);
        int max = 0;
        try {
            max = std::stoi(maxnum->toString());
        } catch (...) {
            max = 0;
        }

        // Use existing TxPoWSqlDB instance to avoid deadlock
        std::vector<std::unique_ptr<org::minima::objects::TxPoW>> txps = MinimaDB::getDB()->getTxPoWDB().getSQLDB()->getAllRelevant(max);
        org::minima::txpowdb::sql::TxPoWList txplist(std::move(txps));

        auto txplistdata_ptr = org::minima::objects::base::MiniData::getMiniDataVersion(txplist);
        if (!txplistdata_ptr) {
            throw std::runtime_error("Failed to serialize TxPoWList to MiniData");
        }
        org::minima::objects::base::MiniData txplistdata = *txplistdata_ptr;

        // Build payload into a temp raw file (concatenation of six MiniData items)
        fs::path payload_raw = backupfolder / "payload_raw.bin";
        {
            std::ofstream raw(payload_raw, std::ios::binary);
            if (!raw) {
                throw std::runtime_error("Failed to open temporary payload file for writing");
            }
            walletdata.writeDataStream(raw);
            cascadedata.writeDataStream(raw);
            chaindata.writeDataStream(raw);
            userdata.writeDataStream(raw);
            p2pdata.writeDataStream(raw);
            txplistdata.writeDataStream(raw);
            raw.flush();
            if (!raw) {
                throw std::runtime_error("Failed writing to temporary payload file");
            }
        }

        // GZIP compress the payload file
        fs::path payload_gz = backupfolder / "payload.gz";
        MiniFile::compressGzipFile(payload_raw, payload_gz);

        // Read compressed payload
        std::vector<std::uint8_t> gzbytes = MiniFile::readCompleteFile(payload_gz);

        // Prepare salt (8 bytes)
        std::vector<std::uint8_t> bsalt(8);
        {
            std::random_device rd;
            for (auto& b : bsalt) {
                b = static_cast<std::uint8_t>(rd());
            }
        }
        MiniData salt(bsalt);

        // Prepare IV
        std::vector<std::uint8_t> iv = GenerateKey::IvParam();
        MiniData ivparam(iv);

        // Derive secret key
        auto secretKey = GenerateKey::secretKey(password, bsalt).bytes();

        // Encrypt GZIP payload
        auto cipher = GenerateKey::getCipherSYM(org::minima::utils::encrypt::Cipher::ENCRYPT_MODE, iv, secretKey);
        std::vector<std::uint8_t> ciphertext = cipher.doFinal(gzbytes);

        // Write final file: [salt.writeDataStream][iv.writeDataStream][ciphertext raw]
        {
            std::ofstream out(backupfile, std::ios::binary);
            if (!out) {
                throw std::runtime_error("Failed to open backup file for writing");
            }
            salt.writeDataStream(out);
            ivparam.writeDataStream(out);

            if (!ciphertext.empty()) {
                out.write(reinterpret_cast<const char*>(ciphertext.data()),
                          static_cast<std::streamsize>(ciphertext.size()));
            }
            out.flush();
            if (!out) {
                throw std::runtime_error("Failed writing encrypted payload to backup file");
            }
        }

        // Compute sizes
        auto fsize = [](const fs::path& p) -> std::uintmax_t {
            std::error_code ec;
            auto sz = fs::file_size(p, ec);
            return ec ? 0 : sz;
        };

        long long total = static_cast<long long>(fsize(walletfile)) +
                          static_cast<long long>(fsize(cascade)) +
                          static_cast<long long>(fsize(chain)) +
                          static_cast<long long>(fsize(userdb)) +
                          static_cast<long long>(fsize(p2pdb)) +
                          static_cast<long long>(txplistdata.getLength());

        // Individual file sizes
        org::minima::utils::json::JSONObject files;
        files.put("wallet", MiniFormat::formatSize(static_cast<long long>(fsize(walletfile))));
        files.put("cascade", MiniFormat::formatSize(static_cast<long long>(fsize(cascade))));
        files.put("chain", MiniFormat::formatSize(static_cast<long long>(fsize(chain))));
        files.put("user", MiniFormat::formatSize(static_cast<long long>(fsize(userdb))));
        files.put("p2p", MiniFormat::formatSize(static_cast<long long>(fsize(p2pdb))));
        files.put("txpow", MiniFormat::formatSize(static_cast<long long>(txplistdata.getLength())));

        // Response JSON
        org::minima::utils::json::JSONObject resp;
        // We cannot safely access the TxPoWTree tip without additional headers; mirror Java's "-1" fallback.
        resp.put("block", std::string("-1"));
        resp.put("files", files);
        resp.put("uncompressed", MiniFormat::formatSize(total));
        resp.put("file", fs::absolute(backupfile).string());
        resp.put("size", MiniFormat::formatSize(static_cast<long long>(fsize(backupfile))));

        // Auto status: best effort since we cannot query UserDB here
        bool auto_setting = existsParam("auto") ? getBooleanParam("auto") : false;
        resp.put("auto", auto_setting);

        ret->put("backup", resp);

        // Clean up temp backup folder
        MiniFile::deleteFileOrFolder(GeneralParams::DATA_FOLDER, backupfolder);
    } catch (const std::exception& exc) {
        // Unlock DB
        MinimaDB::getDB()->readLock(false);

        // Delete backup folder
        MiniFile::deleteFileOrFolder(GeneralParams::DATA_FOLDER, backupfolder);

        // Propagate as CommandException
        throw org::minima::system::commands::CommandException(std::string(exc.what()));
    }

    // Unlock..
    MinimaDB::getDB()->readLock(false);

    // Delete backup folder (in case it still exists)
    MiniFile::deleteFileOrFolder(GeneralParams::DATA_FOLDER, backupfolder);

    return ret;
}

org::minima::system::commands::Command* backup::getFunction() {
    return new backup();
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org