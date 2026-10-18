#include "org/minima/system/commands/backup/mysql.hpp"

#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <filesystem>
#include <stdexcept>
#include <cctype>

// Project headers
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/archive/raw_archive_input.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/userprefs/user_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/b_i_p39.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/mysql/my_s_q_l_connect.hpp"

using org::minima::database::MinimaDB;
using org::minima::database::archive::ArchiveManager;
using org::minima::database::archive::RawArchiveInput;
using org::minima::database::cascade::Cascade;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::database::userprefs::UserDB;
using org::minima::database::wallet::Wallet;
using org::minima::objects::Address;
using org::minima::objects::Coin;
using org::minima::objects::CoinProof;
using org::minima::objects::IBD;
using org::minima::objects::TxBlock;
using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniByte;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::system::Main;
using org::minima::system::commands::CommandRunner;
using org::minima::system::params::GeneralParams;
using org::minima::utils::BIP39;
using org::minima::utils::MiniFile;
using org::minima::utils::MiniFormat;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::utils::mysql::MySQLConnect;

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

mysql::mysql()
    : org::minima::system::commands::Command("mysql",
          "Store and resync the archive data in a MySQL database") {
}

std::string mysql::getFullHelp() const {
    return std::string("\nmysql\n"
        "\n"
        "Export archive data to a MySQL server.\n"
        "\n"
        "The MySQL db can be used to perform a chain re-sync to put users on the correct chain,\n"
        "\n"
        "or a seed re-sync to restore access to lost funds, using the seed phrase.\n"
        "\n"
        "Can query an address for its history of spent and unspent coins.\n"
        "\n"
        "Additionally export the MySQL db to a gzip file for resyncing with 'reset' or 'archive' command.\n"
        "\n"
        "You can use setlogin to auto set the login details\n"
        "\n"
        "host:\n"
        "    The ip:port (or name of Docker container) running the MySQL db.\n"
        "\n"
        "database:\n"
        "    name of the MySQL db being used to store the archive db data.\n"
        "\n"
        "user:\n"
        "    MySQL user to login to as.\n"
        "\n"
        "password:\n"
        "    MySQL password for the user provided.\n"
        "\n"
        "readonly:\n"
        "    true or false, Connect in readonly mode.\n"
        "logs:\n"
        "    Show detailed logs - default true.\n"
        "\n"
        "action:\n"
        "    info : Show the blocks stored in the archive db and compare to the MySQL db.\n"
        "    integrity : Check the block order and block parents are correct in the MySQL db.\n"
        "    update : Update the MySQL db with the latest syncblocks from the node's archive db.\n"
        "    addresscheck : Check the history of all the spent and unspent coins from an address.\n"
        "    setlogin : Set the MySQL login details so you don't need to type them in every time.\n"
        "    clearlogin : Clear MySQL login details.\n"
        "    autobackup : Automatically save archive data to MySQL DB. Use with enable. Also stores all TxPoW the node sees.\n"
        "    findtxpow : Search for an individual TxPoW (only works if autobackup is enabled).\n"
        "    resync : Perform a chain or seed re-sync from the specified MySQL db.\n"
        "             Will shutdown the node so you must restart it once complete.\n"
        "    wipe :  Be careful. Wipe the MySQL db.\n"
        "    h2export : export the MySQL db to an archive gzip file which can be used to resync a node.\n"
        "    h2import : import an archive gzip file to the MySQL db.\n"
        "    rawexport : export the MySQL db to raw.dat file which can be used to resync a node (faster than H2).\n"
        "    rawimport : import a raw.dat to the MySQL db.\n"
        "\n"
        "phrase: (optional)\n"
        "     Use with action:resync. The 24 word seed phrase of the node to re-sync.\n"
        "     If provided, the node will be wiped and re-synced.\n"
        "     If not provided, the node will be re-synced to the chain and will not be wiped.\n"
        "\n"
        "keys: (optional)\n"
        "    If the seed phrase is provided, optionally set the number of keys to create.\n"
        "    Default is 80.\n"
        "\n"
        "keyuses: (optional)\n"
        "    If the seed phrase is provided, optionally set the number of previous uses for each key created.\n"
        "    Default is 1000.\n"
        "\n"
        "address: (optional)\n"
        "    Use with action:addresscheck. The address to check the history of spent and unspent coins for.\n"
        "\n"
        "enable: (optional)\n"
        "    Use with action:autobackup. Automatically save data to MySQL archive DB.\n"
        "\n"
        "file: (optional)\n"
        "    Name or path of the archive gzip file to export to or import from.\n"
        "\n"
        "Examples:\n"
        "\n"
        "mysql ..LOGIN_DETAILS.. action:setlogin\n"
        "\n"
        "mysql host:mysqlhost:port database:archivedb user:archiveuser password:archivepassword action:info\n"
        "\n"
        "mysql host:dockermysql database:archivedb user:archiveuser password:archivepassword action:info\n"
        "\n"
        "mysql (If you have not setlogin)..LOGIN_DETAILS.. action:integrity\n"
        "\n"
        "mysql action:update\n"
        "\n"
        "mysql action:addresscheck address:MxG08..\n"
        "\n"
        "mysql action:resync\n"
        "\n"
        "mysql action:findtxpow txpowid:0x00FFEEDD..\n"
        "\n"
        "mysql action:resync phrase:\"24 WORDS HERE\" keys:90 keyuses:2000\n"
        "\n"
        "mysql action:rawexport file:archivexport-DDMMYY.gzip\n");
}

std::vector<std::string> mysql::getValidParams() const {
    return {
        "action","host","database","user","password","keys","keyuses","phrase",
        "address","txpowid","enable","file","statecheck","logs","maxexport",
        "readonly","startfix","endfix","block"
    };
}

std::unique_ptr<JSONObject> mysql::runCommand() {
    auto ret = getJSONReply();

    if (GeneralParams::IS_MOBILE) {
        throw org::minima::system::commands::CommandException("Sorry - MySQL does not work on Android..");
    }

    std::string action = getParam("action","info");

    // Get login details
    UserDB& udb = MinimaDB::getDB()->getUserDB();
    std::string host, db, user, password;

    bool freshdetails = existsParam("host") && existsParam("database");
    bool autologindetail = udb.getAutoLoginDetailsMySQL();

    if (autologindetail && !freshdetails) {
        host     = udb.getAutoMySQLHost();
        db       = udb.getAutoMySQLDB();
        user     = udb.getAutoMySQLUser();
        password = udb.getAutoMySQLPassword();
    } else {
        host     = getParam("host");
        db       = getParam("database");
        user     = getParam("user");
        password = getParam("password");
    }

    bool logs = getBooleanParam("logs", true);
    bool readonly = getBooleanParam("readonly", false);

    MySQLConnect mysql(host, db, user, password, readonly);
    mysql.init(); // throws on failure

    // Ensure shutdown at end
    auto mysqlShutdownGuard = [&mysql]() noexcept { mysql.shutdown(); };
    try {
        ArchiveManager& arch = MinimaDB::getDB()->getArchive();

        if (action == "info") {
            long long firstblock = mysql.loadFirstBlock();
            long long lastblock  = mysql.loadLastBlock();
            long long total      = firstblock - lastblock;

            auto archLast  = arch.loadLastBlock();
            auto archFirst = arch.loadFirstBlock();

            long long archtotal = 0;
            JSONObject archresp;
            archresp.put("archivestart", static_cast<long long>(-1));
            archresp.put("archiveend", static_cast<long long>(-1));
            archresp.put("archivetotal", static_cast<long long>(0));

            if (archLast && archFirst) {
                archtotal = archFirst->getTxPoW().getBlockNumber().sub(archLast->getTxPoW().getBlockNumber()).getAsLong();
                archresp.put("archivestart", archLast->getTxPoW().getBlockNumber().getAsLong());
                archresp.put("archiveend", archFirst->getTxPoW().getBlockNumber().getAsLong());
                archresp.put("archivetotal", archtotal);
            }

            JSONObject mysqlresp;
            mysqlresp.put("mysqlstart", lastblock);
            mysqlresp.put("mysqlend", firstblock);
            mysqlresp.put("mysqltotal", total);

            JSONObject resp;
            resp.put("archive", archresp);
            resp.put("mysql", mysqlresp);

            bool autobackup = MinimaDB::getDB()->getUserDB().getAutoBackupMySQL();
            resp.put("autobackup", autobackup);

            bool logindetails = MinimaDB::getDB()->getUserDB().getAutoLoginDetailsMySQL();
            resp.put("logindetails", logindetails);

            resp.put("user", user);
            resp.put("password", std::string("***"));
            resp.put("host", host);
            resp.put("database", db);
            resp.put("storealltxpow", GeneralParams::MYSQL_STORE_ALLTXPOW);

            ret->put("response", resp);

        } else if (action == "wipe") {
            mysql.wipeAll();
            ret->put("response", std::string("MySQL DB Wiped.."));

        } else if (action == "setlogin") {
            udb.setAutoMySQLHost(host);
            udb.setAutoMySQLDB(db);
            udb.setAutoMySQLUser(user);
            udb.setAutoMySQLPassword(password);
            udb.setAutoLoginDetailsMySQL(true);

            JSONObject resp;
            resp.put("logindetails", udb.getAutoLoginDetailsMySQL());
            resp.put("user", user);
            resp.put("password", std::string("***"));
            resp.put("host", host);
            resp.put("database", db);
            ret->put("response", resp);

        } else if (action == "clearlogin") {
            udb.setAutoMySQLHost("");
            udb.setAutoMySQLDB("");
            udb.setAutoMySQLUser("");
            udb.setAutoMySQLPassword("");
            udb.setAutoLoginDetailsMySQL(false);

            JSONObject resp;
            resp.put("logindetails", udb.getAutoLoginDetailsMySQL());
            ret->put("response", resp);

        } else if (action == "autobackup") {
            bool enable = getBooleanParam("enable");
            udb.setAutoBackupMySQL(enable);

            if (enable) {
                udb.setAutoMySQLHost(host);
                udb.setAutoMySQLDB(db);
                udb.setAutoMySQLUser(user);
                udb.setAutoMySQLPassword(password);
            } else {
                udb.setAutoMySQLHost("");
                udb.setAutoMySQLDB("");
                udb.setAutoMySQLUser("");
                udb.setAutoMySQLPassword("");
            }

            JSONObject resp;
            resp.put("autobackup", MinimaDB::getDB()->getUserDB().getAutoBackupMySQL());
            ret->put("response", resp);

        } else if (action == "integrity") {
            long long mysqllastblock  = mysql.loadLastBlock();
            long long mysqlfirstblock = mysql.loadFirstBlock();

            (void)mysqlfirstblock; // not used directly

            long long firstblock = -1;
            long long endblock = -1;

            bool havePrev = false;
            MiniData prevTxID = MiniData::ZERO_TXPOWID();
            MiniNumber prevBlockNum = MiniNumber::ZERO();

            long long startload = mysqllastblock;
            while (true) {
                if (logs) {
                    MinimaLogger::log("MySQL Verifying from : " + std::to_string(startload), false);
                }
                auto blocks = mysql.loadBlockRange(MiniNumber(startload));
                if (blocks.empty()) {
                    break;
                }

                for (auto& uptr : blocks) {
                    const TxBlock& block = *uptr;
                    const TxPoW& txp = block.getTxPoW();

                    if (!havePrev) {
                        firstblock = txp.getBlockNumber().getAsLong();
                        havePrev = true;
                    } else {
                        if (!txp.getParentID().isEqual(prevTxID)) {
                            throw org::minima::system::commands::CommandException(
                                "ERROR : block parents are incorrect @ " + txp.getBlockNumber().toString());
                        }
                        if (!txp.getBlockNumber().isEqual(prevBlockNum.increment())) {
                            throw org::minima::system::commands::CommandException(
                                "ERROR : block numbers are incorrect @ " + txp.getBlockNumber().toString());
                        }
                    }

                    prevTxID = txp.getTxPoWIDData();
                    prevBlockNum = txp.getBlockNumber();
                    endblock = txp.getBlockNumber().getAsLong();
                }

                startload = endblock + 1;
            }

            JSONObject resp;
            resp.put("start", firstblock);
            resp.put("end", endblock);
            ret->put("response", resp);

        } else if (action == "update") {
            int size = arch.getSize();
            if (size == 0) {
                throw org::minima::system::commands::CommandException("No blocks in ArchiveDB");
            }

            auto las = arch.loadLastBlock();
            auto fir = arch.loadFirstBlock();
            if (!las || !fir) {
                throw org::minima::system::commands::CommandException("Archive DB missing first or last block");
            }

            long long lastarch = las->getTxPoW().getBlockNumber().getAsLong();
            long long firstarch = fir->getTxPoW().getBlockNumber().getAsLong();
            long long mysqlfirstblock = mysql.loadFirstBlock();

            if (mysqlfirstblock > firstarch - 1) {
                throw org::minima::system::commands::CommandException(
                    "Archive nodes to low.. cannot sync arch:" + std::to_string(firstarch) +
                    " mysql:" + std::to_string(mysqlfirstblock));
            }

            long long startload = mysqlfirstblock;
            if (mysqlfirstblock == -1) {
                startload = 0;
                if (lastarch != 0 && lastarch != 1) {
                    MinimaLogger::log("Archive NOT starting from 0!.. Load from start of archive @ " + std::to_string(lastarch));
                    startload = lastarch;
                }
            }

            bool finished = false;
            bool havePrev = false;
            MiniData prevTxID = MiniData::ZERO_TXPOWID();
            MiniNumber prevBlockNum = MiniNumber::ZERO();

            while (!finished) {
                long long ender = startload + 100;
                if (logs) {
                    MinimaLogger::log("MySQL block transfer from " + std::to_string(startload) +
                                      " to " + std::to_string(ender - 1));
                }

                auto blocks = arch.loadBlockRange(MiniNumber(startload), MiniNumber(ender), false);
                if (blocks.empty()) {
                    MinimaLogger::log("All blocks added");
                    finished = true;
                    break;
                }

                for (auto& uptr : blocks) {
                    const TxBlock& block = *uptr;
                    const TxPoW& txp = block.getTxPoW();

                    if (havePrev) {
                        if (!txp.getParentID().isEqual(prevTxID)) {
                            throw org::minima::system::commands::CommandException(
                                "ERROR : block parents are incorrect @ " + txp.getBlockNumber().toString());
                        }
                        if (!txp.getBlockNumber().isEqual(prevBlockNum.increment())) {
                            throw org::minima::system::commands::CommandException(
                                "ERROR : block numbers are incorrect @ " + txp.getBlockNumber().toString());
                        }
                    } else {
                        havePrev = true;
                    }

                    prevTxID = txp.getTxPoWIDData();
                    prevBlockNum = txp.getBlockNumber();

                    long long saveblock = txp.getBlockNumber().getAsLong();

                    mysql.saveBlock(block);

                    if (saveblock > startload) {
                        startload = saveblock;
                    }
                }
            }

            long long firstblock = mysql.loadFirstBlock();
            long long mylastblock = mysql.loadLastBlock();
            long long total = firstblock - mylastblock;

            auto archlastblock  = arch.loadLastBlock();
            auto archfirstblock = arch.loadFirstBlock();
            long long archtotal = 0;
            if (archlastblock && archfirstblock) {
                archtotal = archfirstblock->getTxPoW().getBlockNumber().sub(
                                archlastblock->getTxPoW().getBlockNumber()).getAsLong();
            }

            JSONObject mysqlresp;
            mysqlresp.put("mysqlstart", mylastblock);
            mysqlresp.put("mysqlend", firstblock);
            mysqlresp.put("mysqltotal", total);

            JSONObject archresp;
            if (archlastblock && archfirstblock) {
                archresp.put("archivestart", archlastblock->getTxPoW().getBlockNumber().getAsLong());
                archresp.put("archiveend", archfirstblock->getTxPoW().getBlockNumber().getAsLong());
                archresp.put("archivetotal", archtotal);
            } else {
                archresp.put("archivestart", static_cast<long long>(-1));
                archresp.put("archiveend", static_cast<long long>(-1));
                archresp.put("archivetotal", static_cast<long long>(0));
            }

            JSONObject resp;
            resp.put("archive", archresp);
            resp.put("mysql", mysqlresp);
            ret->put("response", resp);

        } else if (action == "resync") {
            // Not implementable with provided C++ context (missing processing APIs)
            throw org::minima::system::commands::CommandException(
                "resync is not supported in this C++ build due to missing processing APIs");

        } else if (action == "addresscheck") {
            std::string address = getAddressParam("address");
            std::string statecheck = getParam("statecheck", "");

            // Convert to 0x if looks like Mx and not state index format
            std::string lower = statecheck;
            for (auto& c : lower) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            if (lower.rfind("mx", 0) == 0 && statecheck.find("@") == std::string::npos) {
                statecheck = Address::convertMinimaAddress(statecheck).to0xString();
            }

            JSONObject resp;
            JSONArray inarr;
            JSONArray outarr;
            MiniNumber firstStart = MiniNumber::ZERO();

            while (true) {
                auto blocks = mysql.loadBlockRange(firstStart);
                if (blocks.empty()) {
                    break;
                }

                for (auto& uptr : blocks) {
                    const TxBlock& block = *uptr;
                    const TxPoW& txp = block.getTxPoW();
                    const std::string txpid = txp.getTxPoWID();
                    long long blocknumber = txp.getBlockNumber().getAsLong();

                    std::string date = formatDateMillis(txp.getTimeMilli().getAsLong());

                    const auto& outputs = block.getOutputCoins();
                    for (const Coin& cc : outputs) {
                        if (cc.getAddress().to0xString() == address) {
                            bool found = true;
                            if (!statecheck.empty()) {
                                found = cc.checkForStateVariable(statecheck);
                            }
                            if (found) {
                                JSONObject created;
                                created.put("block", blocknumber);
                                created.put("blockid", txpid);
                                created.put("date", date);
                                created.put("coin", cc.toJSON());
                                outarr.add(created);
                            }
                        }
                    }

                    const auto& inputs = block.getInputCoinProofs();
                    for (const CoinProof& incoin : inputs) {
                        const Coin& c = incoin.getCoin();
                        if (c.getAddress().to0xString() == address) {
                            bool found = true;
                            if (!statecheck.empty()) {
                                found = c.checkForStateVariable(statecheck);
                            }
                            if (found) {
                                JSONObject spent;
                                spent.put("block", blocknumber);
                                spent.put("blockid", txpid);
                                spent.put("date", date);
                                spent.put("coin", c.toJSON());
                                inarr.add(spent);
                            }
                        }
                    }

                    firstStart = txp.getBlockNumber().increment();
                }
            }

            resp.put("created", outarr);
            resp.put("spent", inarr);
            ret->put("coins", resp);

        } else if (action == "h2import") {
            // Not implementable: ArchiveManager C++ header does not expose restoreFromFile/loadDB
            throw org::minima::system::commands::CommandException(
                "h2import is not supported in this C++ build (no ArchiveManager restore/backup APIs)");

        } else if (action == "h2export") {
            // Not implementable: ArchiveManager C++ header does not expose backupToFile/loadDB
            throw org::minima::system::commands::CommandException(
                "h2export is not supported in this C++ build (no ArchiveManager restore/backup APIs)");

        } else if (action == "size") {
            int total = mysql.getCount();
            JSONObject resp;
            resp.put("size", total);
            ret->put("response", resp);

        } else if (action == "rawexport") {
            std::string outfile = getParam("file","archivebackup-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".raw.dat");

            std::filesystem::path rawoutput = MiniFile::createBaseFile(outfile);
            if (std::filesystem::exists(rawoutput)) {
                std::filesystem::remove(rawoutput);
            }

            // Write to a temp file then gzip
            std::filesystem::path tmpfile = rawoutput;
            tmpfile += ".tmp";

            {
                std::ofstream out(tmpfile, std::ios::binary);
                if (!out) {
                    throw org::minima::system::commands::CommandException("Cannot open temp output file for rawexport");
                }

                auto casc = mysql.loadCascade();
                if (casc) {
                    MinimaLogger::log("Cascade found in MySQL..");
                    MiniByte::WriteToStream(out, true);
                    casc->writeDataStream(out);
                } else {
                    MiniByte::WriteToStream(out, false);
                    MinimaLogger::log("No cascade found in MySQL..");
                }

                int total = mysql.getCount();
                MinimaLogger::log("Total records found : " + std::to_string(total));

                if (existsParam("maxexport")) {
                    int max = getNumberParam("maxexport")->getAsInt();
                    MinimaLogger::log("Max export specified.. : " + std::to_string(max));
                    if (total > max) {
                        total = max;
                    }
                }

                MiniNumber tot(total);
                tot.writeDataStream(out);

                int outcounter = 0;

                long long mysqllastblock  = mysql.loadLastBlock();
                long long mysqlfirstblock = mysql.loadFirstBlock();
                (void)mysqlfirstblock;

                long long firstblock = -1;
                long long endblock   = -1;

                long long startload = mysqllastblock;
                int counter = 0;

                while (true) {
                    if (counter % 20 == 0) {
                        if (logs) {
                            MinimaLogger::log("Loading from MySQL @ " + std::to_string(startload));
                        }
                    }

                    auto blocks = mysql.loadBlockRange(MiniNumber(startload));
                    if (blocks.empty()) {
                        break;
                    }

                    for (auto& uptr : blocks) {
                        TxBlock& block = *uptr;
                        block.writeDataStream(out);

                        if (firstblock == -1) {
                            firstblock = block.getTxPoW().getBlockNumber().getAsLong();
                        }
                        endblock = block.getTxPoW().getBlockNumber().getAsLong();

                        outcounter++;
                        if (outcounter >= total) {
                            break;
                        }
                    }

                    if (outcounter >= total) {
                        MinimaLogger::log("Finished loading blocks..");
                        break;
                    }

                    startload = endblock + 1;

                    counter++;
                }

                out.flush();

                // Gzip the temp file to the target
                MiniFile::compressGzipFile(tmpfile, rawoutput);
                std::error_code ec;
                std::filesystem::remove(tmpfile, ec);

                auto fsize = std::filesystem::file_size(rawoutput);

                // Build JSON response
                JSONObject resp;
                resp.put("start", firstblock);
                resp.put("end", endblock);
                resp.put("file", rawoutput.filename().string());
                resp.put("path", std::filesystem::absolute(rawoutput).string());
                resp.put("size", MiniFormat::formatSize(static_cast<long long>(fsize)));

                ret->put("response", resp);
            }

        } else if (action == "reset") {
            JSONObject resp;

            std::string infile = getParam("file");
            // First import the file
            auto resimport = CommandRunner::getRunner()->runSingleCommand("mysql action:rawimport file:" + infile);
            if (!resimport || !resimport->containsKey("status") || !std::any_cast<bool>(resimport->get("status"))) {
                std::string err = resimport && resimport->containsKey("error")
                                  ? std::any_cast<std::string>(resimport->get("error"))
                                  : std::string("unknown");
                throw org::minima::system::commands::CommandException("Error importing data.. " + err);
            }
            resp.put("import", *resimport);

            // And now total resync
            auto resresync = CommandRunner::getRunner()->runSingleCommand("mysql action:resync");
            if (!resresync || !resresync->containsKey("status") || !std::any_cast<bool>(resresync->get("status"))) {
                std::string err = resresync && resresync->containsKey("error")
                                  ? std::any_cast<std::string>(resresync->get("error"))
                                  : std::string("unknown");
                throw org::minima::system::commands::CommandException("Error resyncing.. " + err);
            }
            resp.put("resync", *resresync);

            ret->put("response", resp);

        } else if (action == "rawimport") {
            // NOTE: Main::MYSQL_IMPORTING_NO_ACTION flag not available in C++ headers; we proceed without toggling it.

            try {
                auto timestart = std::chrono::steady_clock::now();

                std::string infile = getParam("file");
                std::filesystem::path fileinfile = MiniFile::createBaseFile(infile);

                RawArchiveInput rawin(fileinfile);
                rawin.connect();

                mysql.wipeAll();

                const Cascade* casc = rawin.getCascade();
                if (casc) {
                    MinimaLogger::log("Cascade found.. ");
                    mysql.saveCascade(*casc);
                }

                int counter = 0;
                while (true) {
                    std::unique_ptr<IBD> syncibd = rawin.getNextIBD();
                    if (!syncibd) {
                        break;
                    }
                    auto& blocks = syncibd->getTxBlocks();
                    if (logs) {
                        if (counter % 10 == 0) {
                            if (!blocks.empty()) {
                                MinimaLogger::log("Loading from RAW Block : " +
                                    blocks.front()->getTxPoW().getBlockNumber().toString());
                            }
                        }
                    }
                    if (blocks.empty()) {
                        break;
                    }

                    for (auto& sptr : blocks) {
                        mysql.saveBlock(*sptr);
                    }

                    counter++;
                }

                rawin.stop();

                auto timediff = std::chrono::steady_clock::now() - timestart;
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timediff).count();

                JSONObject resp;
                resp.put("time", MiniFormat::ConvertMilliToTime(ms));
                ret->put("response", resp);

            } catch (const std::exception& exc) {
                MinimaLogger::log(exc);
                throw org::minima::system::commands::CommandException(exc.what());
            }

        } else if (action == "fixmissing") {
            auto timestart = std::chrono::steady_clock::now();

            std::string infile = getParam("file");
            std::filesystem::path fileinfile = MiniFile::createBaseFile(infile);

            RawArchiveInput rawin(fileinfile);
            rawin.connect();

            long long startchecking = 0;
            if (existsParam("startfix")) {
                startchecking = getNumberParam("startfix")->getAsLong();
            }

            int counter = 0;
            bool savingblocks = false;
            int totalsaved = 0;

            while (true) {
                std::unique_ptr<IBD> syncibd = rawin.getNextIBD();
                if (!syncibd) break;
                auto& blocks = syncibd->getTxBlocks();

                if (logs) {
                    if (counter % 10 == 0) {
                        if (!blocks.empty()) {
                            MinimaLogger::log("Loading from RAW Block : " +
                                              blocks.front()->getTxPoW().getBlockNumber().toString() +
                                              " SAVING:" + std::string(savingblocks ? "true" : "false"),
                                              false);
                        }
                    }
                }

                if (blocks.empty()) {
                    break;
                }

                for (auto& sptr : blocks) {
                    const TxBlock& block = *sptr;
                    long long blocknum = block.getTxPoW().getBlockNumber().getAsLong();

                    if (blocknum > startchecking) {
                        savingblocks = true;

                        auto txblk = mysql.loadBlockFromNum(blocknum);
                        if (!txblk) {
                            mysql.saveBlock(block);
                            totalsaved++;
                            MinimaLogger::log("Save MISSING block : " + std::to_string(blocknum));
                        }
                    }
                }

                counter++;
            }

            rawin.stop();

            auto timediff = std::chrono::steady_clock::now() - timestart;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timediff).count();

            JSONObject resp;
            resp.put("missing", totalsaved);
            resp.put("time", MiniFormat::ConvertMilliToTime(ms));
            ret->put("response", resp);

        } else if (action == "findtxpow") {
            JSONObject resp;

            if (existsParam("txpowid")) {
                std::string txpowid = getParam("txpowid");
                auto txp = mysql.getTxPoW(txpowid);
                if (!txp) {
                    auto txblk = mysql.loadBlockFromID(txpowid);
                    if (!txblk) {
                        resp.put("found", false);
                    } else {
                        resp.put("found", true);
                        resp.put("txpow", txblk->getTxPoW().toJSON());
                    }
                } else {
                    resp.put("found", true);
                    resp.put("txpow", txp->toJSON());
                }
            } else if (existsParam("block")) {
                auto block = getNumberParam("block");
                auto txp = mysql.loadBlockFromNum(block->getAsLong());
                if (!txp) {
                    resp.put("found", false);
                } else {
                    resp.put("found", true);
                    resp.put("txpow", txp->getTxPoW().toJSON());
                }
            } else {
                throw org::minima::system::commands::CommandException(
                    "MUST provide either txpowid or block for findtxpow function");
            }

            ret->put("response", resp);

        } else {
            throw org::minima::system::commands::CommandException("Invalid action : " + action);
        }

    } catch (...) {
        mysqlShutdownGuard();
        throw;
    }
    mysqlShutdownGuard();

    return ret;
}

org::minima::system::commands::Command* mysql::getFunction() {
    return new mysql();
}

void mysql::convertMySQLParams(const std::string& zMySQLDB) {
    try {
        auto atpos = zMySQLDB.find('@');
        if (atpos == std::string::npos) throw std::invalid_argument("missing @");
        std::string user = zMySQLDB.substr(0, atpos);
        std::string db   = zMySQLDB.substr(atpos + 1);

        auto colon = user.find(':');
        if (colon == std::string::npos) throw std::invalid_argument("user:pass separator missing");
        std::string username = user.substr(0, colon);
        std::string password = user.substr(colon + 1);

        auto slash = db.find('/');
        if (slash == std::string::npos) throw std::invalid_argument("host:port/database separator missing");
        std::string dbhost = db.substr(0, slash);
        std::string dbname = db.substr(slash + 1);

        MinimaLogger::log("MYSQL Database Setup : " + username + ":***@" + dbhost + " / " + dbname);

        UserDB& udb = MinimaDB::getDB()->getUserDB();

        GeneralParams::MYSQL_STORE_ALLTXPOW = true;

        udb.setAutoMySQLHost(dbhost);
        udb.setAutoMySQLDB(dbname);
        udb.setAutoMySQLUser(username);
        udb.setAutoMySQLPassword(password);
        udb.setAutoLoginDetailsMySQL(true);

        udb.setAutoBackupMySQL(true);

        if (GeneralParams::MYSQL_DB_COINS) {
            udb.setAutoBackupMySQLCoins(true);
        }

        if (GeneralParams::MYSQL_DB_DELAY != 0) {
            MinimaLogger::log("Waiting " + std::to_string(GeneralParams::MYSQL_DB_DELAY) +
                              " ms before attempting first MySQL connection..");
            std::this_thread::sleep_for(std::chrono::milliseconds(GeneralParams::MYSQL_DB_DELAY));
        }

        MySQLConnect mysql(dbhost, dbname, username, password, true);
        mysql.init();
        mysql.shutdown();

    } catch (const std::exception& exc) {
        MinimaLogger::log("Failed to connect to MySQL DB - MUST be username:password@host:port/database " + std::string(exc.what()));
    }
}

std::string mysql::formatDateMillis(long long millis) {
    using namespace std::chrono;
    system_clock::time_point tp = system_clock::time_point{} + milliseconds(millis);
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S %Z");
    return oss.str();
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org

#ifdef BUILD_MYSQL_COMMAND_MAIN
#include <iostream>
int main(int argc, char* argv[]) {
    org::minima::utils::mysql::MySQLConnect conn("127.0.0.1:3306", "minimadb", "minimauser", "minimapassword");
    try {
        std::cout << "Attemp Connect!" << std::endl;
        conn.init();
        std::cout << "Connected!" << std::endl;
        int count = conn.getCount();
        std::cout << "Rows : " << count << std::endl;
        conn.shutdown();
        std::cout << "Shutdown!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
#endif