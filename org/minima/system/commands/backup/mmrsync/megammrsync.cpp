#include "org/minima/system/commands/backup/mmrsync/megammrsync.hpp"

#include <sstream>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include <thread>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/wallet/wallet.hpp"
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/objects/coin_proof.hpp"
#include "org/minima/objects/i_b_d.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/commands/command_runner.hpp"
#include "org/minima/system/commands/backup/archive.hpp"
#include "org/minima/system/commands/backup/vault.hpp"
#include "org/minima/system/commands/network/connect.hpp"
#include "org/minima/system/commands/network/peers.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_message.hpp"
#include "org/minima/system/params/general_params.hpp"

#include "org/minima/utils/b_i_p39.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/messages/message.hpp"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace backup {
namespace mmrsync {

// ========== MegaMMRSyncData ==========

// MegaMMRSyncData::MegaMMRSyncData(const std::vector<org::minima::objects::base::MiniData>& zAddresses,
//                                  const std::vector<org::minima::objects::base::MiniData>& zPublicKeys)
//     : mAllAddresses(zAddresses), mAllPublicKeys(zPublicKeys) {}

// const std::vector<org::minima::objects::base::MiniData>& MegaMMRSyncData::getAllAddresses() const {
//     return mAllAddresses;
// }
// const std::vector<org::minima::objects::base::MiniData>& MegaMMRSyncData::getAllPublicKeys() const {
//     return mAllPublicKeys;
// }

// void MegaMMRSyncData::writeDataStream(std::ostream& out) {
//     // Version
//     org::minima::objects::base::MiniNumber::WriteToStream(out, 1);
//     // Addresses
//     org::minima::objects::base::MiniNumber::WriteToStream(out, static_cast<int>(mAllAddresses.size()));
//     for (const auto& addr : mAllAddresses) {
//         // MiniData::writeDataStream is non-const; write via a local copy
//         org::minima::objects::base::MiniData tmp = addr;
//         tmp.writeDataStream(out);
//     }
//     // Public Keys
//     org::minima::objects::base::MiniNumber::WriteToStream(out, static_cast<int>(mAllPublicKeys.size()));
//     for (const auto& pk : mAllPublicKeys) {
//         org::minima::objects::base::MiniData tmp = pk;
//         tmp.writeDataStream(out);
//     }
// }

// void MegaMMRSyncData::readDataStream(std::istream& in) {
//     int version = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
//     (void)version; // currently unused
//     mAllAddresses.clear();
//     mAllPublicKeys.clear();
//     int alen = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
//     for (int i = 0; i < alen; ++i) {
//         mAllAddresses.emplace_back(org::minima::objects::base::MiniData::ReadFromStream(in));
//     }
//     int plen = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
//     for (int i = 0; i < plen; ++i) {
//         mAllPublicKeys.emplace_back(org::minima::objects::base::MiniData::ReadFromStream(in));
//     }
// }

// ========== MegaMMRIBD ==========

// MegaMMRIBD::MegaMMRIBD() = default;

// MegaMMRIBD::MegaMMRIBD(std::unique_ptr<org::minima::objects::IBD> zIBD,
//                        std::vector<std::unique_ptr<org::minima::objects::CoinProof>> zAllCoinProofs)
//     : mInitialIBD(std::move(zIBD)), mAllCoinProofs(std::move(zAllCoinProofs)) {}

// MegaMMRIBD::~MegaMMRIBD() = default;
// MegaMMRIBD::MegaMMRIBD(MegaMMRIBD&&) noexcept = default;
// MegaMMRIBD& MegaMMRIBD::operator=(MegaMMRIBD&&) noexcept = default;

// org::minima::objects::IBD& MegaMMRIBD::getIBD() { return *mInitialIBD; }
// const org::minima::objects::IBD& MegaMMRIBD::getIBD() const { return *mInitialIBD; }

// std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& MegaMMRIBD::getAllCoinProofs() { return mAllCoinProofs; }
// const std::vector<std::unique_ptr<org::minima::objects::CoinProof>>& MegaMMRIBD::getAllCoinProofs() const { return mAllCoinProofs; }

// void MegaMMRIBD::setPeersList(const std::string& zPeersList) { mPeersList = zPeersList; }
// std::string MegaMMRIBD::getPeersList() const { return mPeersList; }

// void MegaMMRIBD::writeDataStream(std::ostream& out) {
//     // Version
//     org::minima::objects::base::MiniNumber::WriteToStream(out, 2);
//     // IBD
//     if (!mInitialIBD) {
//         auto tmp = std::make_unique<org::minima::objects::IBD>();
//         tmp->writeDataStream(out);
//     } else {
//         mInitialIBD->writeDataStream(out);
//     }
//     // CoinProofs
//     org::minima::objects::base::MiniNumber::WriteToStream(out, static_cast<int>(mAllCoinProofs.size()));
//     for (auto& cp : mAllCoinProofs) {
//         cp->writeDataStream(out);
//     }
//     // Peers list
//     org::minima::objects::base::MiniString::WriteToStream(out, mPeersList);
// }

// void MegaMMRIBD::readDataStream(std::istream& in) {
//     int version = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
//     (void)version;

//     mInitialIBD = std::make_unique<org::minima::objects::IBD>(org::minima::objects::IBD::ReadFromStream(in));

//     mAllCoinProofs.clear();
//     int len = org::minima::objects::base::MiniNumber::ReadFromStream(in).getAsInt();
//     for (int i = 0; i < len; ++i) {
//         auto cp = org::minima::objects::CoinProof::ReadFromStream(in);
//         if (cp) {
//             mAllCoinProofs.emplace_back(std::move(cp));
//         }
//     }
//     mPeersList = org::minima::objects::base::MiniString::ReadFromStream(in).toString();
// }

// ========== megammrsync Command ==========

megammrsync::megammrsync()
    : Command("megammrsync", "[action:] [host:] (phrase:) (keys:) (keyuses:) (file:) (password:) - Restore from a MegaMMR node") {}

std::string megammrsync::getFullHelp() const {
    return std::string("\nmegammrsync\n"
        "\n"
        "Perform a chain or seed re-sync from a MegaMMR node. Fast.\n"
        "\n"
        "If you are on the wrong chain - all you need to provide is the 'host' to connect to.\n"
        "\n"
        "If you are on a fresh node, with different seed phrase, provide 'host' and 'phrase' to resync with that Wallet.\n"
        "\n"
        "You can load an old backup and resync to the chain tip aswell.\n"
        "\n"
        "The host you connect to MUST be running with -megammr.\n"
        "\n"
        "action:\n"
        "    mydetails : Shows which addresses and public keys you are searching for.\n"
        "    resync : Perform the actual MegaMMR resync.\n"
        "\n"
        "host:\n"
        "    ip:port of the node to sync from. Use with action:resync.\n"
        "\n"
        "phrase: (optional)\n"
        "    To seed re-sync, enter your seed phrase in double quotes. Use with action:resync.\n"
        "    This will replace the current seed phrase of this node. You do NOT have to do this if you still have access to your wallet.\n"
        "\n"
        "anyphrase: (optional)\n"
        "    true or false. If you set a custom seed phrase on startup, you can set this to true. Default is false.\n"
        "\n"
        "keys: (optional) \n"
        "    Number of keys to create if you need to do a seed re-sync. Default is 64.\n"
        "\n"
        "keyuses: (optional) \n"
        "    How many times at most you used your keys..\n"
        "    Every time you re-sync with seed phrase this needs to be higher as Minima Signatures are stateful.\n"
        "    Defaults to 1000 - the max is 262144 for normal keys.\n"
        "\n"
        "file: (optional)\n"
        "    Specify the filename or local path of the backup to restore\n"
        "\n"
        "password: (optional)\n"
        "    Enter the password of the backup \n"
        "\n\n"
        "Examples:\n"
        "\n"
        "megammrsync action:mydetails\n"
        "\n"
        "megammrsync action:resync host:98.65.45.34:9001\n"
        "\n"
        "megammrsync action:resync host:98.65.45.34:9001 file:myoldbakup.bak password:backup_password\n"
        "\n"
        "megammrsync action:resync host:98.65.45.34:9001 phrase:\"YOUR 24 WORD SEED PHRASE\" keyuses:2000\n");
}

std::vector<std::string> megammrsync::getValidParams() const {
    return {"action","host","phrase","anyphrase","keys","keyuses","file","password"};
}

static int32_t read_int32_be_from_buf(const uint8_t* buf) {
    return (static_cast<int32_t>(buf[0]) << 24) |
           (static_cast<int32_t>(buf[1]) << 16) |
           (static_cast<int32_t>(buf[2]) << 8)  |
           (static_cast<int32_t>(buf[3]) << 0);
}

static void write_int32_be_to_stream(std::ostream& out, int32_t v) {
    char b[4];
    b[0] = static_cast<char>((v >> 24) & 0xFF);
    b[1] = static_cast<char>((v >> 16) & 0xFF);
    b[2] = static_cast<char>((v >> 8) & 0xFF);
    b[3] = static_cast<char>((v >> 0) & 0xFF);
    out.write(b, 4);
}

std::unique_ptr<org::minima::utils::json::JSONObject> megammrsync::runCommand() {
    auto ret = getJSONReply();

    std::string action = getParam("action");

    if (action == "mydetails") {
        // Get wallet
        auto* wal = &org::minima::database::MinimaDB::getDB()->getWallet();
        std::vector<std::unique_ptr<ScriptRow>> scrows = wal->getAllAddresses();

        org::minima::utils::json::JSONArray alldets;
        for (const auto& row : scrows) {
            if (row && row->getPublicKey() != "0x00") {
                org::minima::utils::json::JSONObject singledet;
                singledet.put("publickey", row->getPublicKey());
                singledet.put("address", row->getAddress());
                alldets.add(singledet);
            }
        }

        org::minima::utils::json::JSONObject resp;
        resp.put("details", alldets);
        resp.put("size", static_cast<int>(alldets.size()));

        ret->put("response", resp);
        return ret;
    }

    if (action == "resync") {
        // Stop new keys
        org::minima::system::commands::backup::vault::stopAllKeysCreated();

        // Parse host
        std::string fullhost = getParam("host");
        auto connectdata = org::minima::system::commands::network::connect::createConnectMessage(fullhost);
        if (!connectdata) {
            throw org::minima::system::commands::CommandException("Invalid HOST format for resync : " + fullhost);
        }
        std::string host = connectdata->getString("host");
        int port = connectdata->getInteger("port");

        // Test connection to archive
        auto ibdtest = org::minima::system::commands::backup::archive::sendArchiveReq(
            host, port, org::minima::objects::base::MiniNumber::MINUSONE(), 1);
        if (!ibdtest) {
            throw org::minima::system::commands::CommandException(
                std::string("Could not connect to Archive host! @ ") + host + ":" + std::to_string(port));
        } else {
            org::minima::utils::MinimaLogger::log("Check Connection passed!");
        }

        // Keys to create
        int keys = getNumberParam("keys", org::minima::objects::base::MiniNumber(org::minima::database::wallet::Wallet::NUMBER_GETADDRESS_KEYS))->getAsInt();
        int keyuses = getNumberParam("keyuses", org::minima::objects::base::MiniNumber(1000))->getAsInt();

        // Optional seed or file
        std::string file = getParam("file", "");
        std::string phrase = getParam("phrase", "");
        if (!phrase.empty()) {
            bool anyphrase = getBooleanParam("anyphrase", false);

            std::string cleanphrase = phrase;
            if (!anyphrase) {
                cleanphrase = org::minima::utils::BIP39::cleanSeedPhrase(phrase);
            }

            // reset state
            org::minima::system::Main::getInstance()->archiveResetReady(true);

            org::minima::utils::MinimaLogger::log("Resetting all wallet private keys..");
            auto seed = org::minima::utils::BIP39::convertStringToSeed(cleanphrase);

            auto* wallet = &org::minima::database::MinimaDB::getDB()->getWallet();
            wallet->updateSeedRow(cleanphrase, seed.to0xString());

            org::minima::utils::MinimaLogger::log("Creating a total of " + std::to_string(keys) + " keys / addresses..");
            for (int i = 0; i < keys; ++i) {
                org::minima::utils::MinimaLogger::log("Creating key " + std::to_string(i));
                wallet->createNewSimpleAddress(true);
            }
            org::minima::utils::MinimaLogger::log("All keys created..");

            wallet->updateAllKeyUses(keyuses);
        } else if (!file.empty()) {
            std::string command = "restore shutdown:false file:" + file;
            if (existsParam("password")) {
                command += " password:" + getParam("password");
            }
            auto result = org::minima::system::commands::CommandRunner::getRunner()->runSingleCommand(command);
            if (!result || !result->getBoolean("status")) {
                throw org::minima::system::commands::CommandException("Error restoring.. " + (result ? result->toJSONString() : std::string("null")));
            }

            org::minima::system::Main::getInstance()->restoreReadyForSync();
            org::minima::system::Main::getInstance()->archiveResetReady(false);
        } else {
            org::minima::system::Main::getInstance()->archiveResetReady(false);
        }

        // Build details and send request
        MegaMMRSyncData syncdata = getMyDetails();
        auto mibd = sendMegaMMRSyncReq(host, port, syncdata);
        if (!mibd) {
            throw org::minima::system::commands::CommandException("Error getting MegaMMR data from host");
        }

        // Notify MiniDAPPs
        // Create an empty JSONObject object on the stack
        org::minima::utils::json::JSONObject data; 

        // Pass the object 'data' by reference
        org::minima::system::Main::getInstance()->PostNotifyEvent("MDS_RESYNC_START", data);

        // Import all coin proofs
        int csize = static_cast<int>(mibd->getAllCoinProofs().size());
        org::minima::utils::MinimaLogger::log("Import CoinProofs.. " + std::to_string(csize));
        for (const auto& cp : mibd->getAllCoinProofs()) {
            if (!cp) continue;
            // Convert to MiniData
            auto cpdata = org::minima::objects::base::MiniData::getMiniDataVersion(*cp);
            if (!cpdata) continue;
            std::string cmd = "coinimport track:true data:" + cpdata->to0xString();
            (void)org::minima::system::commands::CommandRunner::getRunner()->runSingleCommand(cmd);
        }

        org::minima::utils::MinimaLogger::log("Peers List : " + mibd->getPeersList());

        org::minima::utils::json::JSONObject resp;
        resp.put("message", std::string("MegaMMR sync fininshed.. please restart"));
        resp.put("coins", csize);
        ret->put("response", resp);

        return ret;
    }

    // Default: return help if unknown action
    org::minima::utils::json::JSONObject resp;
    resp.put("error", std::string("Unknown action. Use action:mydetails or action:resync"));
    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* megammrsync::getFunction() {
    return new megammrsync();
}

void megammrsync::updateP2PDB(const std::string& zPeersList) {
    // Best-effort logging; full implementation requires InetSocketAddress type not present in provided headers.
    org::minima::utils::MinimaLogger::log("updateP2PDB skipped (missing InetSocketAddress definition). Peers: " + zPeersList);
}

MegaMMRSyncData megammrsync::getMyDetails() {
    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>> allAddresses;
    std::vector<std::unique_ptr<org::minima::objects::base::MiniData>> allPublicKeys;
    
        auto* wal = &org::minima::database::MinimaDB::getDB()->getWallet();
    
        auto scrows = wal->getAllAddresses();
        for (const auto& row : scrows) {
            if (row && row->isTrack()) {
                allAddresses.emplace_back(std::make_unique<org::minima::objects::base::MiniData>(row->getAddress()));
            }
        }
    
        auto keys = wal->getAllKeys();
        for (const auto& kr : keys) {
            if (kr) {
                allPublicKeys.emplace_back(std::make_unique<org::minima::objects::base::MiniData>(kr->getPublicKey()));
            }
        }
    
    return MegaMMRSyncData(std::move(allAddresses), std::move(allPublicKeys));
}

std::unique_ptr<MegaMMRIBD> megammrsync::getCurrentMegaMMRIBD(const MegaMMRSyncData& zSyncData) {
    auto proofs = getAllCoinProofs(zSyncData);

    auto ibd = std::make_unique<org::minima::objects::IBD>();
    ibd->createCompleteIBD();

    auto mibd = std::make_unique<MegaMMRIBD>(std::move(ibd), std::move(proofs));
    mibd->setPeersList(org::minima::system::commands::network::peers::getPeersList(20));
    return mibd;
}

std::vector<std::unique_ptr<org::minima::objects::CoinProof>>
megammrsync::getAllCoinProofs(const MegaMMRSyncData& zSynData) {
    std::vector<std::unique_ptr<org::minima::objects::CoinProof>> proofs;

    auto coins = searchMegaCoins(zSynData.getAllAddresses(), zSynData.getAllPublicKeys());
    for (const auto& cc : coins) {
        if (!cc) continue;
        std::string cmd = "coinexport coinid:" + cc->getCoinID().to0xString();
        auto coinproofresp = org::minima::system::commands::CommandRunner::getRunner()->runSingleCommand(cmd);
        if (!coinproofresp) continue;

        try {
            // Extract nested "response" and then "data" hex string
            const std::any& respAny = coinproofresp->get("response");
            auto resp = std::any_cast<org::minima::utils::json::JSONObject>(respAny);
            std::string datastr = resp.getString("data");
            org::minima::objects::base::MiniData cpdata(datastr);
            auto newproof = org::minima::objects::CoinProof::convertMiniDataVersion(cpdata);
            if (newproof) {
                proofs.emplace_back(std::move(newproof));
            }
        } catch (const std::bad_any_cast&) {
            // Ignore malformed responses
        }
    }

    return proofs;
}

std::vector<std::unique_ptr<org::minima::objects::Coin>>
megammrsync::searchMegaCoins(const std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& /*zAddresses*/,
                             const std::vector<std::unique_ptr<org::minima::objects::base::MiniData>>& /*zPublicKeys*/)  {
    // Full traversal requires additional headers and DB access patterns; return empty for client-side operation.
    return {};
}

// Cross-platform networking helpers

static bool set_nonblocking_socket(
#ifdef _WIN32
    SOCKET
#else
    int
#endif
    sock, bool nonblock) {
#ifdef _WIN32
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return false;
    if (nonblock) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(sock, F_SETFL, flags) == 0;
#endif
}

static void close_socket(
#ifdef _WIN32
    SOCKET
#else
    int
#endif
    sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

std::unique_ptr<MegaMMRIBD> megammrsync::sendMegaMMRSyncReq(const std::string& zHost,
                                                             int zPort,
                                                             const MegaMMRSyncData& zSyncData) {
    std::unique_ptr<MegaMMRIBD> megaibd;

    try {
        // Create network message
        org::minima::objects::base::MiniByte msgtype(org::minima::system::network::minima::NIOMessage::MSG_MEGAMMRSYNC_REQ());
        auto msg = org::minima::system::network::minima::NIOManager::createNIOMessage(
            msgtype,
            const_cast<MegaMMRSyncData&>(zSyncData) // API requires non-const Streamable&
        );
        if ((msg.getLength() == 0)) {
            throw std::runtime_error("Failed to create NIO message");
        }

        // Serialize MiniData to memory (length-prefixed)
        std::ostringstream oss;
        msg.writeDataStream(oss);
        std::string outbytes = oss.str();

        // Resolve host
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif

        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        struct addrinfo* result = nullptr;
        int gai = getaddrinfo(zHost.c_str(), std::to_string(zPort).c_str(), &hints, &result);
        if (gai != 0 || !result) {
#ifdef _WIN32
            WSACleanup();
#endif
            throw std::runtime_error("getaddrinfo failed");
        }

#ifdef _WIN32
        SOCKET sock = INVALID_SOCKET;
#else
        int sock = -1;
#endif
        struct addrinfo* rp = result;
        for (; rp != nullptr; rp = rp->ai_next) {
#ifdef _WIN32
            SOCKET s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (s == INVALID_SOCKET) continue;
#else
            int s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (s < 0) continue;
#endif
            // Non-blocking connect with timeout 180s
            if (!set_nonblocking_socket(s, true)) {
                close_socket(s);
                continue;
            }

            int connres =
#ifdef _WIN32
                ::connect(s, rp->ai_addr, static_cast<int>(rp->ai_addrlen));
#else
                ::connect(s, rp->ai_addr, rp->ai_addrlen);
#endif
            if (connres < 0) {
#ifdef _WIN32
                int werr = WSAGetLastError();
                if (werr != WSAEWOULDBLOCK && werr != WSAEINPROGRESS) {
                    close_socket(s);
                    continue;
                }
#else
                if (errno != EINPROGRESS) {
                    close_socket(s);
                    continue;
                }
#endif
            }

            // Wait for connect up to 180s
#ifdef _WIN32
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(s, &wfds);
            TIMEVAL tv;
            tv.tv_sec = 180;
            tv.tv_usec = 0;
            int sel = select(0, nullptr, &wfds, nullptr, &tv);
#else
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(s, &wfds);
            struct timeval tv;
            tv.tv_sec = 180;
            tv.tv_usec = 0;
            int sel = select(s + 1, nullptr, &wfds, nullptr, &tv);
#endif
            if (sel <= 0) {
                close_socket(s);
                continue;
            }

            // Connected
            if (!set_nonblocking_socket(s, false)) {
                close_socket(s);
                continue;
            }

            sock =
#ifdef _WIN32
                s
#else
                s
#endif
            ;
            break;
        }

        freeaddrinfo(result);

#ifdef _WIN32
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Could not connect");
        }
#else
        if (sock < 0) {
            throw std::runtime_error("Could not connect");
        }
#endif

        // Send all data
        const char* buf = outbytes.data();
        size_t tosend = outbytes.size();
        while (tosend > 0) {
#ifdef _WIN32
            int sent = ::send(sock, buf, static_cast<int>(tosend), 0);
            if (sent <= 0) {
                close_socket(sock);
                WSACleanup();
                throw std::runtime_error("send failed");
            }
#else
            ssize_t sent = ::send(sock, buf, tosend, 0);
            if (sent <= 0) {
                close_socket(sock);
                throw std::runtime_error("send failed");
            }
#endif
            buf += sent;
            tosend -= static_cast<size_t>(sent);
        }

        // Receive response: first 4 bytes len (MiniData encoding)
        uint8_t lenbuf[4];
        size_t recvd = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
        while (recvd < 4) {
            if (std::chrono::steady_clock::now() > deadline) {
                close_socket(sock);
#ifdef _WIN32
                WSACleanup();
#endif
                throw std::runtime_error("recv timeout (length)");
            }
#ifdef _WIN32
            int r = ::recv(sock, reinterpret_cast<char*>(lenbuf + recvd), static_cast<int>(4 - recvd), 0);
#else
            ssize_t r = ::recv(sock, reinterpret_cast<char*>(lenbuf + recvd), 4 - recvd, 0);
#endif
            if (r <= 0) {
#ifdef _WIN32
                int werr = WSAGetLastError();
                if (werr == WSAEWOULDBLOCK) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
#endif
                close_socket(sock);
#ifdef _WIN32
                WSACleanup();
#endif
                throw std::runtime_error("recv failed (length)");
            }
            recvd += static_cast<size_t>(r);
        }
        int32_t payload_len = read_int32_be_from_buf(lenbuf);
        if (payload_len < 0 || payload_len > (1024 * 1024 * 256)) { // sanity: up to 256MB
            close_socket(sock);
#ifdef _WIN32
            WSACleanup();
#endif
            throw std::runtime_error("invalid payload length");
        }

        std::vector<char> payload(static_cast<size_t>(payload_len));
        size_t need = static_cast<size_t>(payload_len);
        size_t got = 0;
        while (got < need) {
            if (std::chrono::steady_clock::now() > deadline) {
                close_socket(sock);
#ifdef _WIN32
                WSACleanup();
#endif
                throw std::runtime_error("recv timeout (payload)");
            }
#ifdef _WIN32
            int r = ::recv(sock, payload.data() + got, static_cast<int>(need - got), 0);
#else
            ssize_t r = ::recv(sock, payload.data() + got, need - got, 0);
#endif
            if (r <= 0) {
#ifdef _WIN32
                int werr = WSAGetLastError();
                if (werr == WSAEWOULDBLOCK) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
#endif
                close_socket(sock);
#ifdef _WIN32
                WSACleanup();
#endif
                throw std::runtime_error("recv failed (payload)");
            }
            got += static_cast<size_t>(r);
        }

        // Done with socket
        close_socket(sock);
#ifdef _WIN32
        WSACleanup();
#endif

        // Reconstruct MiniData stream: [len(4)][bytes]
        std::ostringstream mdoss;
        write_int32_be_to_stream(mdoss, payload_len);
        mdoss.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        std::string mdbuf = mdoss.str();
        std::istringstream mdiss(mdbuf);
        auto resp = org::minima::objects::base::MiniData::ReadFromStream(mdiss);

        // Now parse payload: [MiniByte type][MegaMMRIBD]
        const auto& bytes = resp.getBytes();
        std::string pay(bytes.begin(), bytes.end());
        std::istringstream pin(pay);

        auto type = org::minima::objects::base::MiniByte::ReadFromStream(pin);
        (void)type; // type is not used in client

        megaibd = std::make_unique<MegaMMRIBD>();
        megaibd->readDataStream(pin);
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log(std::string("MegaMMR Sync connection : ") + exc.what() + " @ " + zHost + ":" + std::to_string(zPort));
        megaibd.reset();
    }

    return megaibd;
}

} // namespace mmrsync
} // namespace backup
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org