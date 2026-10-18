#include "org/minima/system/commands/backup/archive.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <typeinfo>
#include <vector>
#include <sys/time.h>

#ifdef _WIN32
  #ifndef NOMINMAX // Fix: Add #ifndef guard
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")

  // Fix: Undefine conflicting Windows macros
  #ifdef TRUE
    #undef TRUE
  #endif
  #ifdef FALSE
    #undef FALSE
  #endif
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

// Project headers
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/archive/raw_archive_input.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/cascade/cascade_node.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/address.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/system/main.hpp"
#include "org/minima/system/brains/tx_po_w_processor.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/network/connect.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/system/network/webhooks/notify_manager.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/b_i_p39.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/message_listener.hpp"

using org::minima::database::MinimaDB;
using org::minima::database::archive::ArchiveManager;
using org::minima::database::archive::RawArchiveInput;
using org::minima::database::cascade::Cascade;
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
using org::minima::system::brains::TxPoWProcessor;
using org::minima::system::commands::CommandException;
using org::minima::system::commands::CommandRunner;
using org::minima::system::commands::network::connect;
using org::minima::system::network::minima::NIOManager;
using org::minima::system::network::minima::NIOMessage;
using org::minima::system::network::webhooks::NotifyManager;
using org::minima::system::params::GeneralParams;
using org::minima::utils::BIP39;
using org::minima::utils::MiniFile;
using org::minima::utils::MiniFormat;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;
using org::minima::utils::json::JSONObject;
using org::minima::utils::messages::Message;
using org::minima::utils::messages::MessageListener;

// Static member definitions
bool org::minima::system::commands::backup::archive::H2_TEMPARCHIVE = true;
std::unique_ptr<ArchiveManager> org::minima::system::commands::backup::archive::STATIC_TEMPARCHIVE = nullptr;
std::unique_ptr<RawArchiveInput> org::minima::system::commands::backup::archive::STATIC_RAW = nullptr;

namespace {

#ifdef _WIN32
struct WinsockInit {
    WinsockInit() {
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (res != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(res));
        }
    }
    ~WinsockInit() {
        WSACleanup();
    }
};
#endif

// Send all bytes helper
bool send_all(int sockfd, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(sockfd, reinterpret_cast<const char*>(data + sent), static_cast<int>(len - sent), 0);
#else
        ssize_t n = ::send(sockfd, data + sent, len - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

// Recv all bytes helper
bool recv_all(int sockfd, uint8_t* data, size_t len) {
    size_t recvd = 0;
    while (recvd < len) {
#ifdef _WIN32
        int n = ::recv(sockfd, reinterpret_cast<char*>(data + recvd), static_cast<int>(len - recvd), 0);
#else
        ssize_t n = ::recv(sockfd, data + recvd, len - recvd, 0);
#endif
        if (n <= 0) return false;
        recvd += static_cast<size_t>(n);
    }
    return true;
}

int connect_host_port(const std::string& host, int port, int timeout_ms) {
    // Resolve address
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portstr = std::to_string(port);
    struct addrinfo* result = nullptr;

    int gai = getaddrinfo(host.c_str(), portstr.c_str(), &hints, &result);
    if (gai != 0) {
        return -1;
    }

    int sockfd = -1;

    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        // Create socket
#ifdef _WIN32
        SOCKET s = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == INVALID_SOCKET) {
            continue;
        }
        // Set timeouts
        DWORD tv = static_cast<DWORD>(timeout_ms);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        // Connect
        if (::connect(s, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
            sockfd = static_cast<int>(s);
            break;
        }
        ::closesocket(s);
#else
        int s = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1) {
            continue;
        }
        // Set timeouts
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        // Connect
        if (::connect(s, rp->ai_addr, rp->ai_addrlen) == 0) {
            sockfd = s;
            break;
        }
        ::close(s);
#endif
    }

    freeaddrinfo(result);
    return sockfd;
}

} // anonymous namespace

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {

archive::archive()
    : org::minima::system::commands::Command(
          "archive",
          "[action:] (host:) (phrase:) (keys:) (keyuses:) - Resync your chain with seed phrase if necessary (otherwise wallet remains the same)") {
}

std::vector<std::string> archive::getValidParams() const {
    return std::vector<std::string>{
        "action","host","phrase","anyphrase","keys","keyuses","file","address","statecheck","logs","maxexport"
    };
}

std::string archive::getFullHelp() const {
    std::ostringstream ss;
    ss
    << "\narchive\n"
    << "\n"
    << "Perform a chain or seed re-sync from an archive node or archive export file.\n"
    << "\n"
    << "A chain re-sync will put your node on the correct chain.\n"
    << "\n"
    << "Use a chain re-sync if your node has been offline for too long and cannot catchup. Seed Phrase is not required.\n"
    << "\n"
    << "A seed re-sync will wipe the wallet, re-generate your private keys and restore your coins.\n"
    << "\n"
    << "Only do a seed re-sync if you have lost your node and do not have a backup.\n"
    << "\n"
    << "You can also perform checks on your archive db or archive file and check an address.\n"
    << "\n"
    << "action:\n"
    << "    integrity : Check the integrity of your archive db. No host required.\n"
    << "    inspect : inspect an archive export .gzip file. If 'last:1', the file can re-sync any node from genesis.\n"
    << "    export : Export your archive db to a .gzip file. \n"
    << "    exportraw : Export your archive db to a raw .dat file (recommended).\n"
    << "    resync : Use with 'host' to do a chain or seed re-sync from an archive node. \n"
    << "    import : Use with 'file' to do a chain or seed re-sync using a .dat or .gzip archive file. \n"
    << "    addresscheck : check your archive db for spent and unspent coins at a specific address.\n"
    << "\n"
    << "host: (optional) \n"
    << "    ip:port of the archive node to sync from. Use with action:resync.\n"
    << "\n"
    << "file: (optional) \n"
    << "    name or path of the archive export gzip file to export/import/inspect.\n"
    << "\n"
    << "phrase: (optional)\n"
    << "    To seed re-sync, enter your seed phrase in double quotes. Use with action:import or resync.\n"
    << "    This will replace the current seed phrase of this node. You do NOT have to do this if you still have access to your wallet.\n"
    << "    In this case, use action:import or resync without 'phrase' to get on the correct chain.\n"
    << "\n"
    << "anyphrase: (optional)\n"
    << "    true or false. If you set a custom seed phrase on startup, you can set this to true. Default is false.\n"
    << "\n"
    << "keys: (optional) \n"
    << "    Number of keys to create if you need to do a seed re-sync. Default is 64.\n"
    << "\n"
    << "keyuses: (optional) \n"
    << "    How many times at most you used your keys..\n"
    << "    Every time you re-sync with seed phrase this needs to be higher as Minima Signatures are stateful.\n"
    << "    Defaults to 1000 - the max is 262144 for normal keys.\n"
    << "\n"
    << "address: (optional) \n"
    << "    The wallet or script address to search for in the archive. Use with action:addresscheck.\n"
    << "    If using a script address, also provide an address or public key in the 'statecheck' parameter.\n"
    << "    Use with action:addresscheck.\n"
    << "\n"
    << "statecheck: (optional) \n"
    << "    Data to search for in a coin's state variables.\n"
    << "    Combine with a script address in the 'address' parameter to search for coins locked in a contract.\n"
    << "\n"
    << "logs: (optional) \n"
    << "    true or false. Show detailed logs, default false.\n"
    << "\n"
    << "maxexport: (optional) \n"
    << "    How many blocks to export to a raw .dat file. Useful for testing purposes.\n"
    << "\n"
    << "Examples:\n"
    << "\n"
    << "archive action:integrity\n"
    << "\n"
    << "archive action:inspect file:archiveexport-ddmmyy.gzip\n"
    << "\n"
    << "archive action:exportraw file:archiveexport-ddmmyy.raw.dat\n"
    << "\n"
    << "archive action:resync host:89.98.89.98:9001\n"
    << "\n"
    << "archive action:import file:archiveexport-ddmmyy.raw.dat\n"
    << "\n"
    << "archive action:import file:archiveexport-ddmmyy.raw.dat phrase:\"YOUR 24 WORD SEED PHRASE\" keyuses:2000\n"
    << "\n"
    << "archive action:addresscheck address:0xFED.. statecheck:0xABC..\n";
    return ss.str();
}

std::unique_ptr<JSONObject> archive::runCommand() {
    auto ret = getJSONReply();

    const std::string action = getParam("action");

    // ArchiveManager reference
    ArchiveManager& arch = MinimaDB::getDB()->getArchive();

    if (action == "integrity") {
        MinimaLogger::log("Checking Archive DB.. this may take some time..");

        // Last block in DB
        std::unique_ptr<TxBlock> starterblock = arch.loadLastBlock();

        bool startcheck = true;
        MiniNumber lastlog = MiniNumber::ZERO();
        MiniNumber start   = MiniNumber::ZERO();

        if (!starterblock) {
            MinimaLogger::log("You have no Archive blocks..");
            startcheck = false;
        } else {
            lastlog = starterblock->getTxPoW().getBlockNumber();
            start   = lastlog;

            MiniNumber startblocknumber = starterblock->getTxPoW().getBlockNumber();
            if (startblocknumber.isEqual(MiniNumber::ONE())) {
                MinimaLogger::log("ArchiveDB starts at root");
            }
        }

        // Cascade details
        MiniNumber cascstart = MiniNumber::MINUSONE();
        JSONObject cascjson;
        std::unique_ptr<Cascade> dbcasc = arch.loadCascade();
        if (dbcasc) {
            cascjson.put("exists", true);
            cascstart = dbcasc->getTip()->getTxPoW().getBlockNumber();
            cascjson.put("tip", cascstart.toString());
            cascjson.put("length", dbcasc->getLength());
        } else {
            cascjson.put("exists", false);
        }

        MiniData parenthash;
        bool has_parenthash = false;
        MiniNumber parentnum = MiniNumber::ZERO();
        int errorsfound = 0;
        int total = 0;
        MiniNumber archstart = start;
        MiniNumber archend = archstart;

        JSONObject archjson;
        archjson.put("start", archstart.toString());

        while (startcheck) {
            if (lastlog.isLess(start.sub(MiniNumber(2000)))) {
                MinimaLogger::log("Now checking from  " + start.toString());
                lastlog = start;
            }

            MiniNumber end = start.add(MiniNumber::TWOFIVESIX());

            // include parent using start.decrement()
            std::vector<std::unique_ptr<TxBlock>> blocks = arch.loadBlockRange(start.decrement(), end, false);

            for (auto& uptr_block : blocks) {
                TxBlock& block = *uptr_block;
                archend = block.getTxPoW().getBlockNumber();
                total++;

                if (!has_parenthash) {
                    parenthash = block.getTxPoW().getTxPoWIDData();
                    has_parenthash = true;
                    parentnum = block.getTxPoW().getBlockNumber();
                    lastlog = parentnum;
                    archstart = parentnum;

                    std::string when = currentTimeString(block.getTxPoW().getTimeMilli().getAsLong());
                    MinimaLogger::log("ArchiveDB blocks resync start at block " + parentnum.toString() + " @ " + when);
                } else {
                    if (!block.getTxPoW().getBlockNumber().isEqual(parentnum.increment())) {
                        MinimaLogger::log("Incorrect child block @ " + block.getTxPoW().getBlockNumber().toString() +
                                          " parent:" + parentnum.toString());
                        errorsfound++;
                    } else if (!block.getTxPoW().getParentID().isEqual(parenthash)) {
                        MinimaLogger::log("Parent hash incorrect @ " + block.getTxPoW().getBlockNumber().toString());
                        errorsfound++;
                    }

                    parenthash = block.getTxPoW().getTxPoWIDData();
                    parentnum = block.getTxPoW().getBlockNumber();
                }
            }

            if (blocks.empty()) {
                break;
            }

            start = parentnum.increment();
        }

        archjson.put("end", archend.toString());
        archjson.put("blocks", total);

        MiniNumber startresync = archstart;
        bool validlist = false;
        if (!archstart.isEqual(MiniNumber::ONE())) {
            if (cascstart.isMoreEqual(archstart.sub(MiniNumber::ONE())) && cascstart.isLessEqual(archend)) {
                validlist = true;
            }
            startresync = cascstart;
        } else {
            validlist = true;
        }

        JSONObject resp;
        resp.put("message", "Archive integrity check completed");
        resp.put("cascade", cascjson);
        resp.put("archive", archjson);
        resp.put("valid", validlist);
        if (!validlist) {
            resp.put("notvalid", "Your cascade and blocks do not line up.. new cascade required.. pls restart Minima");
        }
        resp.put("from", startresync.toString());
        resp.put("errors", errorsfound);

        if (errorsfound > 0) {
            resp.put("recommend", "There are errors in your Archive DB blocks - you should wipe then resync with a valid host");
        } else {
            resp.put("recommend", "Your ArchiveDB is correct and has no errors.");
        }

        ret->put("response", resp);
        return ret;
    } else if (action == "resync") {
        // Listener
        MessageListener* minimalistener = Main::getInstance()->getMinimaListener();

        // Host
        const std::string fullhost = getParam("host");

        // Local?
        bool usinglocal = false;
        if (fullhost == LOCAL_ARCHIVE) {
            if (H2_TEMPARCHIVE && !STATIC_TEMPARCHIVE) {
                throw CommandException("No Local STATIC Archive DB Found..");
            }
            usinglocal = true;
        }

        std::shared_ptr<org::minima::utils::messages::Message> connectdata;
        std::string host;
        int port = 0;
        if (!usinglocal) {
            connectdata = connect::createConnectMessage(fullhost);
            if (!connectdata) {
                throw CommandException("Invalid HOST format for resync : " + fullhost);
            }
            host = connectdata->getString("host");
            port = connectdata->getInteger("port");
        }

        // Keys and uses
        int keys = getNumberParam("keys", MiniNumber(Wallet::NUMBER_GETADDRESS_KEYS))->getAsInt();
        int keyuses = getNumberParam("keyuses", MiniNumber(1000))->getAsInt();

        // Pre-check connection
        if (!usinglocal) {
            auto ibdtest = sendArchiveReq(host, port, MiniNumber::MINUSONE());
            if (!ibdtest) {
                throw CommandException("Could not connect to Archive host! @ " + host + ":" + std::to_string(port));
            }
        }

        // Notify MiniDAPPs
        // Fix: Pass JSONObject{} by value, not std::make_shared
        JSONObject data; // Create a named variable (lvalue)
        Main::getInstance()->PostNotifyEvent("MDS_RESYNC_START", data); 

        // Phrase handling
        MiniData seed;
        std::string phrase = getParam("phrase", "");
        if (!phrase.empty()) {
            bool anyphrase = getBooleanParam("anyphrase", false);
            std::string cleanphrase = phrase;
            if (!anyphrase) {
                cleanphrase = BIP39::cleanSeedPhrase(phrase);
            }

            Main::getInstance()->archiveResetReady(true);

            MinimaLogger::log("Resetting all wallet private keys..");

            seed = BIP39::convertStringToSeed(cleanphrase);

            Wallet& wallet = MinimaDB::getDB()->getWallet();

            wallet.updateSeedRow(cleanphrase, seed.to0xString());

            MinimaLogger::log("Creating a total of " + std::to_string(keys) + " keys / addresses..");
            for (int i = 0; i < keys; ++i) {
                NotifyListener(minimalistener, "Creating key " + std::to_string(i));
                MinimaLogger::log("Creating key " + std::to_string(i));
                auto dummy = wallet.createNewSimpleAddress(true);
                (void)dummy;
            }
            MinimaLogger::log("All keys created..");

            wallet.updateAllKeyUses(keyuses);
        } else {
            Main::getInstance()->archiveResetReady(false);
        }

        // Loop through chain
        MiniNumber startblock = MiniNumber::ZERO();
        long long starttime = 0;
        MiniNumber endblock = MiniNumber::ZERO();
        bool firstrun = true;
        MiniNumber firstStart = MiniNumber::ZERO();

        long long lastlogmessage = 0;
        int counter = 0;

        MinimaLogger::log("System clean..");

        while (true) {
            counter++;

            if (counter % 10 == 0) {
                // Fix: resetMemFull() is private, comment out
                // Main::getInstance()->resetMemFull();
            }

            std::unique_ptr<IBD> ibd;

            if (!usinglocal) {
                ibd = sendArchiveReq(host, port, startblock);
            } else {
                if (H2_TEMPARCHIVE) {
                    ibd = std::make_unique<IBD>();
                    ibd->createArchiveIBD(startblock, *STATIC_TEMPARCHIVE, true);
                } else {
                    if (STATIC_RAW) {
                        ibd = STATIC_RAW->getNextIBD();
                    }
                }
            }

            if (!ibd) {
                ibd = std::make_unique<IBD>();
            }

            if (startblock.isEqual(MiniNumber::ZERO()) && ibd->hasCascade()) {
                MinimaLogger::log("Cascade Received.. " + ibd->getCascade()->getTip()->getTxPoW().getBlockNumber().toString());
                MinimaDB::getDB()->setIBDCascade(*ibd->getCascade());
                MinimaDB::getDB()->getArchive().checkCascadeRequired(*ibd->getCascade());
            }

            int size = static_cast<int>(ibd->getTxBlocks().size());

            if (size > 0) {
                TxBlock& startblk = *ibd->getTxBlocks()[0];
                if (firstrun) {
                    firstrun = false;
                    firstStart = startblk.getTxPoW().getBlockNumber();
                }

                TxBlock& last = *ibd->getTxBlocks()[size - 1];
                endblock = last.getTxPoW().getBlockNumber();

                startblock = endblock.increment();
                starttime = last.getTxPoW().getTimeMilli().getAsLong();

                // Notify
                NotifyListener(minimalistener,
                               "Loading " + startblk.getTxPoW().getBlockNumber().toString() +
                               " @ " + currentTimeString(startblk.getTxPoW().getTimeMilli().getAsLong()));
            } else {
                MinimaLogger::log("No Archive TxBlocks left..");
            }

            // Post it
            std::shared_ptr<IBD> sibd(ibd.release());
            // Fix: Use . not -> for getTxPoWProcessor()
            Main::getInstance()->getTxPoWProcessor().postProcessArchiveIBD(sibd, "0x00");

            // Do we print a log..
            long long now = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch()).count());
            if ((now - lastlogmessage) > 5000) {
                MinimaLogger::log("IBD Processed.. next block:" + startblock.toString() +
                                  " @ " + currentTimeString(starttime));
                lastlogmessage = now;
            }

            if (size == 0) {
                break;
            }
        }

        NotifyListener(minimalistener, "All blocks loaded.. pls wait");
        MinimaLogger::log("All Archive data received and processed.");

        JSONObject resp;
        resp.put("message", "Archive sync completed.");
        resp.put("start", firstStart.toString());
        resp.put("end", endblock.toString());
        ret->put("response", resp);

        // Persist DB state
        MinimaDB::getDB()->saveAllDB();

        return ret;
    } else if (action == "export") {
        // Not supported in this C++/SQLite build (Java used H2 backupToFile)
        throw CommandException("export (H2 gzip) is not supported in this build. Use action:exportraw instead.");
    } else if (action == "exportraw") {
        if (arch.getSize() == 0) {
            throw CommandException("No blocks in ArchiveDB");
        }

        bool logs = getBooleanParam("logs", false);

        std::string file = getParam("file", "archivebackup-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".raw.dat");
        std::filesystem::path rawoutput = MiniFile::createBaseFile(file);
        if (std::filesystem::exists(rawoutput)) {
            std::error_code ec;
            std::filesystem::remove(rawoutput, ec);
        }

        // ensure parent folder
        std::filesystem::path parent = rawoutput.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
        }

        // We'll write to a temporary uncompressed file then gzip it
        std::filesystem::path tmp = rawoutput;
        tmp += ".tmp";

        std::ofstream ofs(tmp, std::ios::binary);
        if (!ofs) throw CommandException("Cannot open temp file for write: " + tmp.string());

        // Load first/last block numbers
        int mysqllastblock = arch.loadLastBlock()->getTxPoW().getBlockNumber().getAsInt();
        int mysqlfirstblock = arch.loadFirstBlock()->getTxPoW().getBlockNumber().getAsInt();

        MinimaLogger::log("Exporting ArchiveDB to RAW block format..");

        bool allowcascade = true;
        if (mysqllastblock == 1) {
            allowcascade = false;
            MinimaLogger::log("Archive starts from 1 no need to use a cascade..");
        }

        std::unique_ptr<Cascade> casc = arch.loadCascade();
        if (casc && allowcascade) {
            MinimaLogger::log("Cascade found in Archive DB..");
            MiniByte(MiniByte::TRUE()).writeDataStream(ofs); // Create a non-const copy
            casc->writeDataStream(ofs);

            mysqllastblock = casc->getTip()->getTxPoW().getBlockNumber().getAsInt() + 1;
            MinimaLogger::log("Save blocks from cascade tip onwards.. " + std::to_string(mysqllastblock - 1));
        } else {
            MiniByte(MiniByte::FALSE()).writeDataStream(ofs); // Create a non-const copy
            MinimaLogger::log("No cascade added");
        }

        int total = mysqlfirstblock - mysqllastblock + 1;
        MinimaLogger::log("Add records from  : " + std::to_string(mysqllastblock));
        MinimaLogger::log("Total records to add : " + std::to_string(total));

        if (existsParam("maxexport")) {
            int max = getNumberParam("maxexport")->getAsInt();
            MinimaLogger::log("Max export specified.. : " + std::to_string(max));
            if (total > max) total = max;
        }

        MiniNumber tot(total);
        tot.writeDataStream(ofs);

        int outcounter = 0;
        long long firstblock = -1;
        long long endblock = -1;
        TxBlock* lastblock = nullptr;

        long long startload = static_cast<long long>(mysqllastblock) - 1;
        int counter = 0;
        while (true) {
            if (counter % 20 == 0) {
                if (logs) {
                    MinimaLogger::log("Loading from Archive @ " + std::to_string(startload));
                }
            }

            std::vector<std::unique_ptr<TxBlock>> blocks =
                arch.loadBlockRange(MiniNumber(startload), MiniNumber(startload).add(MiniNumber::HUNDRED()), false);

            if (blocks.empty()) {
                break;
            }

            for (auto& uptr_block : blocks) {
                TxBlock& block = *uptr_block;
                block.writeDataStream(ofs);
                if (lastblock == nullptr) {
                    firstblock = block.getTxPoW().getBlockNumber().getAsLong();
                }
                lastblock = &block;
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

            startload = endblock;

            counter++;
        }

        ofs.flush();
        ofs.close();

        // Compress
        MiniFile::compressGzipFile(tmp, rawoutput);
        // Remove tmp
        std::error_code ec;
        std::filesystem::remove(tmp, ec);

        JSONObject resp;
        resp.put("start", firstblock);
        resp.put("end", endblock);
        resp.put("total", outcounter);
        resp.put("file", rawoutput.filename().string());
        resp.put("path", std::filesystem::absolute(rawoutput).string());
        resp.put("size", MiniFormat::formatSize(static_cast<long long>(std::filesystem::file_size(rawoutput))));
        ret->put("response", resp);
        return ret;
    } else if (action == "import") {
        std::string file = getParam("file");

        std::filesystem::path restorefile = MiniFile::createBaseFile(file);
        if (!std::filesystem::exists(restorefile)) {
            throw CommandException("Restore file doesn't exist : " + std::filesystem::absolute(restorefile).string());
        }

        bool h2import = true;
        if (file.size() >= 4 && file.substr(file.size()-4) == ".dat") {
            h2import = false;
        }

        if (h2import) {
            // Not supported in this C++/SQLite build (Java used H2 restoreFromFile)
            throw CommandException("H2 archive import (.gzip) is not supported in this build. Use a RAW .dat file.");
        } else {
            MinimaLogger::log("RAW archive import started..");
            H2_TEMPARCHIVE = false;

            STATIC_RAW = std::make_unique<RawArchiveInput>(restorefile);
            STATIC_RAW->connect();

            // Build the resync command to reuse the same flow (including phrase/keys/anyphrase)
            std::string command = "archive action:resync host:" + LOCAL_ARCHIVE;
            if (existsParam("phrase")) {
                std::string phr = getParam("phrase");
                // Quote as Java did
                command += " phrase:\"" + phr + "\"";
            }
            if (existsParam("keys")) {
                command += " keys:" + getParam("keys");
            }
            if (existsParam("keyuses")) {
                command += " keyuses:" + getParam("keyuses");
            }
            if (existsParam("anyphrase")) {
                command += " anyphrase:" + getParam("anyphrase");
            }

            // Run the resync command through the runner
            auto runner = CommandRunner::getRunner();
            std::unique_ptr<JSONArray> res = runner->runMultiCommand(command);

            // Shutdown RAW reader
            STATIC_RAW->stop();
            STATIC_RAW.reset();

            // Package response
            JSONObject resp;
            if (res && res->size() > 0) {
                const std::any& a0 = res->at(0);
                if (a0.type() == typeid(JSONObject)) {
                    resp.put("archiveresync", std::any_cast<JSONObject>(a0));
                } else {
                    resp.put("archiveresync", "resync started");
                }
            } else {
                resp.put("archiveresync", "resync started");
            }
            ret->put("response", resp);
            return ret;
        }
    } else if (action == "importraw") {
        std::string file = getParam("file");

        std::filesystem::path restorefile = MiniFile::createBaseFile(file);
        if (!std::filesystem::exists(restorefile)) {
            throw CommandException("Restore file doesn't exist : " + std::filesystem::absolute(restorefile).string());
        }

        Main::getInstance()->archiveResetReady(false);

        RawArchiveInput rawin(restorefile);
        rawin.connect();

        int ibdcount = 0;
        while (true) {
            std::unique_ptr<IBD> syncibd = rawin.getNextIBD();
            int size = static_cast<int>(syncibd->getTxBlocks().size());
            if (size == 0) {
                break;
            }

            // Fix: Get processor by reference, use . operator
            TxPoWProcessor& proc = Main::getInstance()->getTxPoWProcessor();
            std::shared_ptr<IBD> sibd(std::move(syncibd));
            proc.postProcessArchiveIBD(sibd, "0x00");
            ibdcount++;

            if (ibdcount % 20 == 0) {
                TxBlock& block = *sibd->getTxBlocks().back();
                long long starttime = block.getTxPoW().getTimeMilli().getAsLong();
                MinimaLogger::log("Processing block:" + block.getTxPoW().getBlockNumber().toString() +
                                  " @ " + currentTimeString(starttime));
                
                // Fix: resetMemFull() is private, comment out
                // Main::getInstance()->resetMemFull();
            }
        }

        rawin.stop();

        MinimaLogger::log("All blocks processed..");

        JSONObject resp;
        resp.put("message", "Archive sync completed");
        ret->put("response", resp);

        MinimaDB::getDB()->saveAllDB();
        return ret;
    } else if (action == "inspectraw") {
        long long timestart = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count());

        std::string infile = getParam("file");
        std::filesystem::path fileinfile = MiniFile::createBaseFile(infile);

        RawArchiveInput rawin(fileinfile);
        rawin.connect();

        if (const Cascade* c = rawin.getCascade()) {
            MinimaLogger::log("Cascade found.. ");
            (void)c;
        }

        int counter = 0;
        MiniNumber lasttxblock = MiniNumber::ZERO();

        while (true) {
            std::unique_ptr<IBD> syncibd = rawin.getNextIBD();
            auto& blocks = syncibd->getTxBlocks();

            if (counter % 10 == 0) {
                if (!blocks.empty()) {
                    MinimaLogger::log("Loading from RAW Block : " + blocks[0]->getTxPoW().getBlockNumber().toString(), false);
                }
            }

            if (blocks.empty()) {
                break;
            }

            bool exit = false;
            for (auto& uptr_block : blocks) {
                TxBlock& block = *uptr_block;
                MiniNumber blocknum = block.getTxPoW().getBlockNumber();
                if (!blocknum.isEqual(lasttxblock.increment())) {
                    MinimaLogger::log("INVALID non sequential blocks.. lastblock:" + lasttxblock.toString() +
                                      " new:" + blocknum.toString());
                    exit = true;
                    break;
                }
                lasttxblock = blocknum;
            }

            counter++;
            if (exit) break;
        }

        rawin.stop();

        long long timenow = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count());
        long long timediff = timenow - timestart;

        JSONObject resp;
        resp.put("time", MiniFormat::ConvertMilliToTime(timediff));
        ret->put("response", resp);
        return ret;
    } else if (action == "inspect") {
        std::string file = getParam("file", "");
        if (!file.empty()) {
            // Unsupported: inspecting an H2 gzip file in this build
            throw CommandException("inspect from file (.gzip) is not supported in this build. Omit 'file' to inspect the current DB.");
        }

        // Inspect current DB
        JSONObject resp;
        JSONObject jcasc;
        JSONObject jarch;

        resp.put("cascade", jcasc);
        resp.put("archive", jarch);

        jcasc.put("exists", false);
        jcasc.put("start", "-1");
        jcasc.put("length", "-1");
        jarch.put("first", "-1");
        jarch.put("last", "-1");
        jarch.put("size", "-1");

        if (auto casc = arch.loadCascade()) {
            jcasc.put("exists", true);
            jcasc.put("start", casc->getTip()->getTxPoW().getBlockNumber().toString());
            jcasc.put("length", casc->getLength());
        }

        if (auto first = arch.loadFirstBlock()) {
            jarch.put("first", first->getTxPoW().getBlockNumber().toString());
        }
        if (auto last = arch.loadLastBlock()) {
            jarch.put("last", last->getTxPoW().getBlockNumber().toString());
        }

        jarch.put("size", arch.getSize());

        ret->put("response", resp);
        return ret;
    } else if (action == "addresscheck") {
        std::string address = getAddressParam("address");

        std::string statecheck = getParam("statecheck", "");
        {
            std::string lower = statecheck;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!statecheck.empty()) {
                bool looksMx = (lower.rfind("mx", 0) == 0);
                bool hasAt = (statecheck.find('@') != std::string::npos);
                if (looksMx && !hasAt) {
                    statecheck = Address::convertMinimaAddress(statecheck).to0xString();
                }
            }
        }

        JSONObject resp;
        JSONArray inarr;
        JSONArray outarr;

        ArchiveManager& adb = MinimaDB::getDB()->getArchive();
        std::unique_ptr<TxBlock> startblock = adb.loadLastBlock();

        MiniNumber firstStart = MiniNumber::ZERO();
        bool canstart = true;
        if (!startblock) {
            canstart = false;
        } else {
            firstStart = startblock->getTxPoW().getBlockNumber().decrement();
            MinimaLogger::log("Start archive @ " + firstStart.toString());
        }

        while (canstart) {
            std::vector<std::unique_ptr<TxBlock>> mysqlblocks =
                adb.loadBlockRange(firstStart, firstStart.add(MiniNumber::HUNDRED()), false);

            if (mysqlblocks.empty()) {
                break;
            }

            for (auto& uptr_block : mysqlblocks) {
                TxBlock& block = *uptr_block;

                firstStart = block.getTxPoW().getBlockNumber();

                TxPoW const& txp = block.getTxPoW();
                long long blocknumber = txp.getBlockNumber().getAsLong();

                std::string date = currentTimeString(txp.getTimeMilli().getAsLong());

                // Created (outputs)
                for (const Coin& cc : block.getOutputCoins()) {
                    if (cc.getAddress().to0xString() == address) {
                        bool found = true;
                        if (!statecheck.empty()) {
                            found = cc.checkForStateVariable(statecheck);
                        }
                        if (found) {
                            MinimaLogger::log("BLOCK " + std::to_string(blocknumber) + " CREATED COIN : " + cc.toString());

                            JSONObject created;
                            created.put("block", blocknumber);
                            created.put("date", date);
                            created.put("datemilli", txp.getTimeMilli().toString());
                            created.put("coin", cc.toJSON());
                            outarr.add(created);
                        }
                    }
                }

                // Spent (inputs)
                for (const CoinProof& incoin : block.getInputCoinProofs()) {
                    if (incoin.getCoin().getAddress().to0xString() == address) {
                        bool found = true;
                        if (!statecheck.empty()) {
                            found = incoin.getCoin().checkForStateVariable(statecheck);
                        }
                        if (found) {
                            MinimaLogger::log("BLOCK " + std::to_string(blocknumber) + " SPENT COIN : " + incoin.getCoin().toString());

                            JSONObject spent;
                            spent.put("block", blocknumber);
                            spent.put("date", date);
                            spent.put("datemilli", txp.getTimeMilli().toString());
                            spent.put("coin", incoin.getCoin().toJSON());
                            inarr.add(spent);
                        }
                    }
                }
            }
        }

        MinimaLogger::log("All checks complete..");

        resp.put("created", outarr);
        resp.put("spent", inarr);
        ret->put("coins", resp);
        return ret;
    } else {
        throw CommandException("Invalid action : " + action);
    }
}

org::minima::system::commands::Command* archive::getFunction() {
    return new archive();
}

void archive::NotifyListener(MessageListener* zListener, const std::string& zMessage) {
    if (zListener != nullptr) {
        JSONObject data;
        data.put("message", zMessage);

        JSONObject notify;
        notify.put("event", "ARCHIVEUPDATE");
        notify.put("data", data);

        auto msg = std::make_shared<Message>(NotifyManager::NOTIFY_POST);
        msg->addObject("notify", notify);

        zListener->processMessage(msg);
    }
}

std::unique_ptr<IBD> archive::sendArchiveReq(const std::string& zHost, int zPort, const MiniNumber& zStartBlock) {
    return sendArchiveReq(zHost, zPort, zStartBlock, 3);
}

std::unique_ptr<IBD> archive::sendArchiveReq(const std::string& zHost, int zPort, const MiniNumber& zStartBlock, int zAttempts) {
    std::unique_ptr<IBD> ibd;

    int attempts = 0;

    while (attempts < zAttempts) {
        try {
            // Construct NIO message payload: MiniData containing message type + payload
            MiniData msg = NIOManager::createNIOMessage(NIOMessage::MSG_ARCHIVE_REQ(), zStartBlock);

            // Serialize MiniData to bytes (length-prefixed)
            std::ostringstream oss(std::ios::binary);
            msg.writeDataStream(oss);
            std::string outbound = oss.str();

#ifdef _WIN32
            WinsockInit winit;
#endif
            int sock = connect_host_port(zHost, zPort, 10000);
            if (sock < 0) {
                throw std::runtime_error("connect failed");
            }

            // Send
            if (!send_all(sock, reinterpret_cast<const uint8_t*>(outbound.data()), outbound.size())) {
#ifdef _WIN32
                ::closesocket(sock);
#else
                ::close(sock);
#endif
                throw std::runtime_error("send failed");
            }

            // Read response MiniData (length-prefixed: 4 bytes big-endian)
            uint8_t lenbuf[4];
            if (!recv_all(sock, lenbuf, 4)) {
#ifdef _WIN32
                ::closesocket(sock);
#else
                ::close(sock);
#endif
                throw std::runtime_error("recv failed (len)");
            }
            uint32_t be_len = (static_cast<uint32_t>(lenbuf[0]) << 24) |
                              (static_cast<uint32_t>(lenbuf[1]) << 16) |
                              (static_cast<uint32_t>(lenbuf[2]) << 8)  |
                              (static_cast<uint32_t>(lenbuf[3]));
            std::vector<uint8_t> data(be_len);
            if (!recv_all(sock, data.data(), be_len)) {
#ifdef _WIN32
                ::closesocket(sock);
#else
                ::close(sock);
#endif
                throw std::runtime_error("recv failed (data)");
            }

#ifdef _WIN32
            ::closesocket(sock);
#else
            ::close(sock);
#endif

            // Reassemble full MiniData stream (len + bytes) for parsing
            std::vector<uint8_t> full;
            full.reserve(4 + data.size());
            full.insert(full.end(), lenbuf, lenbuf + 4);
            full.insert(full.end(), data.begin(), data.end());

            std::istringstream iss(std::string(reinterpret_cast<const char*>(full.data()), full.size()), std::ios::binary);

            MiniData resp = MiniData::ReadFromStream(iss);

            // Now parse the content: first a MiniByte type, then IBD
            std::istringstream biss(std::string(reinterpret_cast<const char*>(resp.getBytes().data()), resp.getBytes().size()), std::ios::binary);
            MiniByte type = MiniByte::ReadFromStream(biss);
            (void)type; // not used locally

            IBD parsed = IBD::ReadFromStream(biss);
            ibd = std::make_unique<IBD>(std::move(parsed));
            break;

        } catch (const std::exception& exc) {
            MinimaLogger::log(std::string("Archive connection : ") + exc.what() + " @ " + zHost + ":" + std::to_string(zPort));

            ibd.reset();
            attempts++;

            if (attempts < zAttempts) {
                MinimaLogger::log(std::to_string(attempts) + " Attempts > Wait 10 seconds and re-attempt..");
                std::this_thread::sleep_for(std::chrono::seconds(10));
                MinimaLogger::log("Re-attempt started..");
            }
        }
    }

    return ibd;
}

std::string archive::currentTimeString(long long ms) {
    return MiniFormat::ConvertMilliToTime(ms);
}

} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org