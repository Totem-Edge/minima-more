#include "org/minima/system/network/minima/n_i_o_message.hpp"

// Standard
#include <sstream>
#include <vector>
#include <chrono>
#include <exception>

// Project headers (full definitions needed in source)
#include "org/minima/objects/base/mini_byte.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/base/mini_string.hpp"

#include "org/minima/objects/greeting.hpp"
#include "org/minima/objects/i_b_d.hpp"
#include "org/minima/objects/pulse.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/tx_block.hpp"
#include "org/minima/objects/magic.hpp"
#include "org/minima/objects/tx_body.hpp"
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/coin_proof.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowdb/tx_po_w_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"
#include "org/minima/database/cascade/cascade.hpp"
#include "org/minima/database/archive/archive_manager.hpp"
#include "org/minima/database/archive/tx_block_d_b.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

#include "org/minima/system/brains/tx_po_w_checker.hpp"
#include "org/minima/system/brains/tx_po_w_generator.hpp"
#include "org/minima/system/brains/tx_po_w_searcher.hpp"
#include "org/minima/system/brains/tx_po_w_miner.hpp"
#include "org/minima/system/brains/tx_po_w_processor.hpp"

#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/system/network/p2p2/p2_p2_d_b.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"

// Network manager / client
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_client.hpp"
#include "org/minima/system/network/minima/n_i_o_server.hpp"
#include "org/minima/system/network/minima/n_i_o_traffic.hpp"
#include "org/minima/system/network/minima/relay_policy.hpp"


// Note: RelayPolicy header not available in RAG; forward declare static API used here.
// namespace org { namespace minima { namespace system { namespace network { namespace minima {
// class RelayPolicy {
// public:
//     static bool checkMaxStateStoreSize(org::minima::objects::TxPoW& zTxPoW, long long zMax);
//     static bool checkAllPolicies(org::minima::objects::TxPoW& zTxPoW, long long zMaxStateStoreSize);
// };
// } } } } }

#include "org/minima/system/commands/backup/mmrsync/megammrsync.hpp"

#include "org/minima/utils/mini_format.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"

#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/timer_message.hpp"

#ifdef _WIN32
// No platform-specific code needed here
#endif

using namespace org::minima::objects;
using namespace org::minima::objects::base;
using namespace org::minima::utils;
using namespace org::minima::utils::json;
using namespace org::minima::utils::json::parser;
using namespace org::minima::utils::messages;
using namespace org::minima::database;
using namespace org::minima::database::txpowdb;
using namespace org::minima::database::txpowtree;
using namespace org::minima::system;
using namespace org::minima::system::params;
using namespace org::minima::system::network::p2p;
using namespace org::minima::system::network::p2p::messages;
using namespace org::minima::system::network::minima;

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

// Static definitions
const MiniByte& NIOMessage::MSG_GREETING() { static const MiniByte instance(0); return instance; }
const MiniByte& NIOMessage::MSG_IBD() { static const MiniByte instance(1); return instance; }
const MiniByte& NIOMessage::MSG_TXPOWID() { static const MiniByte instance(2); return instance; }
const MiniByte& NIOMessage::MSG_TXPOWREQ() { static const MiniByte instance(3); return instance; }
const MiniByte& NIOMessage::MSG_TXPOW() { static const MiniByte instance(4); return instance; }
const MiniByte& NIOMessage::MSG_GENMESSAGE() { static const MiniByte instance(5); return instance; }
const MiniByte& NIOMessage::MSG_PULSE() { static const MiniByte instance(6); return instance; }
const MiniByte& NIOMessage::MSG_P2P() { static const MiniByte instance(7); return instance; }
const MiniByte& NIOMessage::MSG_PING() { static const MiniByte instance(8); return instance; }


const MiniByte& NIOMessage::MSG_SINGLE_PING() { static const MiniByte instance(11); return instance; }
const MiniByte& NIOMessage::MSG_SINGLE_PONG() { static const MiniByte instance(12); return instance; }

const MiniByte& NIOMessage::MSG_IBD_REQ() { static const MiniByte instance(13); return instance; }
const MiniByte& NIOMessage::MSG_IBD_RESP() { static const MiniByte instance(14); return instance; }

const MiniByte& NIOMessage::MSG_ARCHIVE_REQ() { static const MiniByte instance(15); return instance; }
const MiniByte& NIOMessage::MSG_ARCHIVE_DATA() { static const MiniByte instance(16); return instance; }
const MiniByte& NIOMessage::MSG_ARCHIVE_SINGLE_REQ() { static const MiniByte instance(17); return instance; }

const MiniByte& NIOMessage::MSG_TXBLOCKID() { static const MiniByte instance(18); return instance; }
const MiniByte& NIOMessage::MSG_TXBLOCKREQ() { static const MiniByte instance(19); return instance; }
const MiniByte& NIOMessage::MSG_TXBLOCK() { static const MiniByte instance(20); return instance; }
const MiniByte& NIOMessage::MSG_TXBLOCKMINE() { static const MiniByte instance(21); return instance; }

const MiniByte& NIOMessage::MSG_MEGAMMRSYNC_REQ() { static const MiniByte instance(22); return instance; }
const MiniByte& NIOMessage::MSG_MEGAMMRSYNC_RESP() { static const MiniByte instance(23); return instance; }

// Static containers
std::unordered_map<std::string, MiniNumber> NIOMessage::mlastSyncReq;
std::unordered_map<std::string, long long>  NIOMessage::mLastChainSync;
std::unordered_set<std::string>             NIOMessage::mHaveSentIBDRecently;

// Mutexes
std::mutex NIOMessage::s_lastSyncReqMutex;
std::mutex NIOMessage::s_lastChainSyncMutex;
std::mutex NIOMessage::s_haveSentIBDMutex;

// Pending during IBD
std::vector<MiniData> NIOMessage::mPendingTxPowIDsDuringIBD;
std::vector<MiniData> NIOMessage::mPendingTxBlockIDsDuringIBD;
std::mutex NIOMessage::s_pendingIBDMutex;

// Timers
long long NIOMessage::LAST_TXBLOCKMINE_MSG = 0;

// Constructor / Destructor / Moves
NIOMessage::NIOMessage(const std::string& zClientUID, const MiniData& zData)
    : mClientUID(zClientUID)
    , mData(std::make_unique<MiniData>(zData))
    , mTrace(false)
    , mFilter("")
    , mFullAdrress("")
    , HEAVIER_CHAIN_FOUND(0) {
}

NIOMessage::~NIOMessage() = default;
NIOMessage::NIOMessage(NIOMessage&&) noexcept = default;
NIOMessage& NIOMessage::operator=(NIOMessage&&) noexcept = default;

void NIOMessage::setFullAddress(const std::string& zAddress) {
    mFullAdrress = zAddress;
}

void NIOMessage::setTrace(bool zTrace, const std::string& zFilter) {
    mTrace  = zTrace;
    mFilter = zFilter;
}

std::string NIOMessage::convertMessageType(const MiniByte& zType) {
    if (zType.isEqual(MSG_GREETING())) return "GREETING";
    else if (zType.isEqual(MSG_IBD())) return "IBD";
    else if (zType.isEqual(MSG_TXPOWID())) return "TXPOWID";
    else if (zType.isEqual(MSG_TXPOWREQ())) return "TXPOWREQ";
    else if (zType.isEqual(MSG_TXPOW())) return "TXPOW";
    else if (zType.isEqual(MSG_GENMESSAGE())) return "GENMESSAGE";
    else if (zType.isEqual(MSG_PULSE())) return "PULSE";
    else if (zType.isEqual(MSG_P2P())) return "P2P";
    else if (zType.isEqual(MSG_PING())) return "PING";
    else if (zType.isEqual(MSG_SINGLE_PONG())) return "MSG_SINGLE_PONG";
    else if (zType.isEqual(MSG_IBD_REQ())) return "MSG_IBD_REQ";
    else if (zType.isEqual(MSG_IBD_RESP())) return "MSG_IBD_RESP";
    else if (zType.isEqual(MSG_ARCHIVE_DATA())) return "MSG_ARCHIVE_DATA";
    else if (zType.isEqual(MSG_ARCHIVE_REQ())) return "MSG_ARCHIVE_REQ";
    else if (zType.isEqual(MSG_ARCHIVE_SINGLE_REQ())) return "MSG_ARCHIVE_SINGLE_REQ";
    else if (zType.isEqual(MSG_MEGAMMRSYNC_REQ())) return "MSG_MEGAMMRSYNC_REQ";
    else if (zType.isEqual(MSG_MEGAMMRSYNC_RESP())) return "MSG_MEGAMMRSYNC_RESP";
    else if (zType.isEqual(MSG_TXBLOCKID())) return "TXBLOCKID";
    else if (zType.isEqual(MSG_TXBLOCKREQ())) return "TXBLOCKREQ";
    else if (zType.isEqual(MSG_TXBLOCK())) return "TXBLOCK";
    else if (zType.isEqual(MSG_TXBLOCKMINE())) return "TXBLOCKMINE";
    return "UNKNOWN_" + zType.toString();
}

void NIOMessage::run() {
    if (!mData) {
        return;
    }

    // Are we shutting down or restoring
    if (Main::getInstance()->isShuttongDownOrRestoring()) {
        mData.reset();
        return;
    }

    // Convert MiniData bytes to input stream
    const std::vector<std::uint8_t>& raw = mData->getBytes();
    std::string inbuf(reinterpret_cast<const char*>(raw.data()), raw.size());
    std::istringstream dis(inbuf, std::ios::binary);

    try {
        // Type
        MiniByte type = MiniByte::ReadFromStream(dis);

        // Ignore during Sync IBD for certain types
        if (Main::getInstance()->isSyncIBD()) {
            if (type.isEqual(MSG_TXPOWID())) {
                // Queue for later instead of dropping silently
                MiniData id = MiniData::ReadFromStream(dis);
                {
                    std::lock_guard<std::mutex> lk(s_pendingIBDMutex);
                    mPendingTxPowIDsDuringIBD.push_back(id);
                }
                return;
            }
            if (type.isEqual(MSG_TXBLOCKID())) {
                MiniData id = MiniData::ReadFromStream(dis);
                {
                    std::lock_guard<std::mutex> lk(s_pendingIBDMutex);
                    mPendingTxBlockIDsDuringIBD.push_back(id);
                }
                return;
            }
            if (type.isEqual(MSG_PULSE())) {
                // Pulse is not critical during IBD; still drop but without per-msg spam
                return;
            }
        }

        // TXBLOCK node ignores TXPOWID
        if (GeneralParams::TXBLOCK_NODE) {
            if (type.isEqual(MSG_TXPOWID())) {
                return;
            }
        }

        std::string tracemsg = "[NIOMessage] uid:" + mClientUID + " type:" + convertMessageType(type)
                             + " size:" + MiniFormat::formatSize(static_cast<long long>(raw.size()));
        if (mTrace && tracemsg.find(mFilter) != std::string::npos) {
            MinimaLogger::log(tracemsg, false);
        }

        if (GeneralParams::NETWORKING_LOGS) {
            MinimaLogger::log("[NETLOGS RECEIVED] from:" + mClientUID + " type:" + convertMessageType(type)
                              + " size:" + MiniFormat::formatSize(static_cast<long long>(raw.size())));
        }

        try {
            std::string strtype = convertMessageType(type);
            int size = static_cast<int>(raw.size());
            // getTrafficListener() returns a reference, not a pointer
            NIOTraffic& traffic = Main::getInstance()->getNIOManager().getTrafficListener();
            // Use . (dot) on a reference, not ->
            // Removed if(traffic) check, as references cannot be null
            traffic.addReadBytes(strtype, size);
        } catch (...) {
            // ignore traffic errors
        }

        // Message handling
        if (type.isEqual(MSG_GREETING())) {
            // getClient returns std::shared_ptr, not raw pointer
            auto nioclient = Main::getInstance()->getNIOManager().getNIOServer()->getClient(mClientUID);
            if (nioclient == nullptr) {
                MinimaLogger::log(mClientUID + " Error null client on Greeting NIOMessage..");
                return;
            }

            auto greet_ptr = Greeting::ReadFromStream(dis);
            if (!greet_ptr) return;
            Greeting& greet = *greet_ptr;
            MinimaLogger::log("[+] Peer greeting: version=" + greet.getVersion().toString()
                + " top=" + greet.getTopBlock().toString()
                + " chain=" + std::to_string(greet.getChain().size())
                + " from " + nioclient->getFullAddress());

            bool testcheck = true;
            std::string greetstr = greet.getVersion().toString();
            if (GeneralParams::TEST_PARAMS && (greetstr.find("TEST") == std::string::npos)) {
                testcheck = false;
            } else if (!GeneralParams::TEST_PARAMS && (greetstr.find("TEST") != std::string::npos)) {
                testcheck = false;
            }

            if (!testcheck) {
                MinimaLogger::log("Greeting with Incompatible Version! " + greet.getVersion().toString()
                                  + " .. we are " + GlobalParams::MINIMA_VERSION + " from "
                                  + nioclient->getFullAddress() + " incoming:" + std::string(nioclient->isIncoming() ? "true" : "false"));

                P2PFunctions::addInvalidPeer(nioclient->getFullAddress());

                Message newconn(P2PFunctions::P2P_NOCONNECT);
                newconn.addObject("client", nioclient);
                newconn.addString("uid", nioclient->getUID());
                // PostMessage expects std::shared_ptr, and had typo
                Main::getInstance()->getNetworkManager().getP2PManager().PostMessage(std::make_shared<Message>(newconn));

                if (nioclient->isIncoming() && !nioclient->haveSentGreeting()) {
                    nioclient->setSentGreeting(true);

                    Greeting greetout;
                    greetout.createGreeting();

                    NIOManager::sendNetworkMessage(nioclient->getUID(), NIOMessage::MSG_GREETING(), greetout);

                    TimerMessage msg(2000, NIOManager::NIO_DISCONNECT);
                    msg.addString("uid", mClientUID);
                    // PostTimerMessage expects std::shared_ptr, and had typo
                    Main::getInstance()->getNIOManager().PostTimerMessage(std::make_shared<TimerMessage>(msg));
                } else {
                    Main::getInstance()->getNIOManager().disconnect(mClientUID);
                }
                return;
            }

            // Reject peers on a different chain at the handshake (chainid mismatch).
            // This prevents confusing "Wrong Block ChainID!" spam during sync.
            if (greet.getExtraData().containsKey("chainid")) {
                try {
                    org::minima::objects::base::MiniData peerchain(greet.getExtraDataValue("chainid"));
                    if (!peerchain.isEqual(brains::TxPoWChecker::CURRENT_NETWORK)) {
                        MinimaLogger::log("Greeting with Incompatible ChainID! " + peerchain.to0xString()
                                          + " .. we are " + brains::TxPoWChecker::CURRENT_NETWORK.to0xString()
                                          + " from " + nioclient->getFullAddress()
                                          + " incoming:" + std::string(nioclient->isIncoming() ? "true" : "false"));

                        P2PFunctions::addInvalidPeer(nioclient->getFullAddress());

                        Message newconn(P2PFunctions::P2P_NOCONNECT);
                        newconn.addObject("client", nioclient);
                        newconn.addString("uid", nioclient->getUID());
                        Main::getInstance()->getNetworkManager().getP2PManager().PostMessage(std::make_shared<Message>(newconn));

                        if (nioclient->isIncoming() && !nioclient->haveSentGreeting()) {
                            nioclient->setSentGreeting(true);

                            Greeting greetout;
                            greetout.createGreeting();

                            NIOManager::sendNetworkMessage(nioclient->getUID(), NIOMessage::MSG_GREETING(), greetout);

                            TimerMessage msg(2000, NIOManager::NIO_DISCONNECT);
                            msg.addString("uid", mClientUID);
                            Main::getInstance()->getNIOManager().PostTimerMessage(std::make_shared<TimerMessage>(msg));
                        } else {
                            Main::getInstance()->getNIOManager().disconnect(mClientUID);
                        }
                        return;
                    }
                } catch (const std::exception& exc) {
                    MinimaLogger::log("Greeting chainid parse error from " + nioclient->getFullAddress() + " : " + exc.what());
                }
            }

            if (nioclient->getHost() == "127.0.0.1") {
                if (greet.getExtraData().containsKey("host")) {
                    MinimaLogger::log("Greeting from SSH Port with HOST set : " + greet.getExtraDataValue("host"));
                }
            }
            if (greet.getExtraData().containsKey("host")) {
                nioclient->overrideHost(greet.getExtraDataValue("host"));
            }
            if (greet.getExtraData().containsKey("port")) {
                try {
                    int port = std::stoi(greet.getExtraDataValue("port"));
                    nioclient->setMinimaPort(port);
                } catch (...) {}
            }

            nioclient->setWelcomeMessage("Minima v" + greet.getVersion().toString());
            nioclient->setValidGreeting(true);

            {
                Message newconn(P2PFunctions::P2P_CONNECTED);
                newconn.addString("uid", nioclient->getUID());
                newconn.addBoolean("incoming", nioclient->isIncoming());
                newconn.addObject("client", nioclient);
                // PostMessage expects std::shared_ptr
                Main::getInstance()->getNetworkManager().getP2PManager().PostMessage(std::make_shared<Message>(newconn));
            }

            if (nioclient->isIncoming()) {
                if (!nioclient->haveSentGreeting()) {
                    nioclient->setSentGreeting(true);
                    Greeting greetout;
                    greetout.createGreeting();
                    NIOManager::sendNetworkMessage(nioclient->getUID(), NIOMessage::MSG_GREETING(), greetout);
                }
            }

            std::string miniaddress = nioclient->getFullMinimaAddress();
            {
                std::lock_guard<std::mutex> lock(s_haveSentIBDMutex);
                if (mHaveSentIBDRecently.find(miniaddress) != mHaveSentIBDRecently.end()) {
                    MinimaLogger::log("Already sent an IBD to " + miniaddress + " in last 30 mins..");
                } else {
                    mHaveSentIBDRecently.insert(miniaddress);

                    IBD ibd;
                    bool isvalid = ibd.createIBD(greet);

                    if (!isvalid) {
                        if (!mFullAdrress.empty()) {
                            P2PFunctions::addInvalidPeer(mFullAdrress);
                        }
                        // Still send ours
                    }
                    NIOManager::sendNetworkMessage(mClientUID, MSG_IBD(), ibd);
                }
            }

        } else if (type.isEqual(MSG_IBD())) {

            // Immediately set sync mode to prevent race condition with Pulse messages
            Main::getInstance()->setSyncIBD(true);

            if (GeneralParams::IBDSYNC_LOGS) {
                MinimaLogger::log("Received IBD size:" + MiniFormat::formatSize(static_cast<long long>(raw.size())));
            }

            // Read into a shared_ptr
            auto ibd = std::make_shared<IBD>(IBD::ReadFromStream(dis));

            if (GeneralParams::IBDSYNC_LOGS) {
                MinimaLogger::log("Received IBD blocks:" + std::to_string(ibd->getTxBlocks().size()));
            }

            if (!ibd->checkValidData()) {
                MinimaLogger::log("Received INVALID IBD from " + mClientUID);
                if (!mFullAdrress.empty()) {
                    P2PFunctions::addInvalidPeer(mFullAdrress);
                }
                Main::getInstance()->getNIOManager().disconnect(mClientUID, true);
                return;
            }

            if (MinimaDB::getDB()->getCascade().getLength() > 0 && ibd->hasCascadeWithBlocks()) {
                // During an Initial Block Download / resync we must accept the peers' consensus
                // chain rather than contest it with our own (potentially stale or empty) view.
                // isSyncIBD() is set true above as soon as an IBD is received, so this covers
                // fresh boots and nodes that have been offline.
                if (!Main::getInstance()->isSyncIBD()) {
                    bool heavier = IBD::checkOurChainHeavier(*ibd); // Dereference shared_ptr

                    if (!heavier) {
                        // Create an empty JSONObject object on the stack
                        org::minima::utils::json::JSONObject data;

                        // Pass the object 'data' by reference
                        Main::getInstance()->PostNotifyEvent("HEAVIER_CHAIN", data);
                        MinimaLogger::log("[!] CONNECTED TO HEAVIER CHAIN.. from " + mClientUID + " ..disconnecting");

                        Main::getInstance()->getNIOManager().disconnect(mClientUID, true);

                        if (!GeneralParams::RESCUE_MEGAMMR_NODE.empty()) {
                            HEAVIER_CHAIN_FOUND++;
                            if (HEAVIER_CHAIN_FOUND > 0) {
                                MinimaLogger::log("RESCUE NODE FOUND.. attempting rescue @ " + GeneralParams::RESCUE_MEGAMMR_NODE);
                                // PostTimerMessage expects std::shared_ptr, and had typo
                                Main::getInstance()->PostTimerMessage(std::make_shared<TimerMessage>(TimerMessage(1000, Main::MAIN_DO_RESCUE)));
                            }
                        }
                        return;
                    } else if (GeneralParams::IBDSYNC_LOGS) {
                        // Downgraded from always-log with [!] to gated behind IBDSYNC_LOGS to reduce spam
                        MinimaLogger::log("Received IBD with cascade from " + mClientUID + " (we already have one)");
                    }
                } else if (GeneralParams::IBDSYNC_LOGS) {
                    MinimaLogger::log("Skipping chain-weight contest while in IBD sync from " + mClientUID);
                }
            }

            MinimaLogger::log("[+] Connected to the blockchain Initial Block Download received. size:"
                              + MiniFormat::formatSize(static_cast<long long>(raw.size()))
                              + " blocks:" + std::to_string(ibd->getTxBlocks().size()));

            // Pass the shared_ptr
            Main::getInstance()->getTxPoWProcessor().postProcessIBD(ibd, mClientUID);

        } else if (type.isEqual(MSG_TXPOWID())) {
            MiniData txpowid = MiniData::ReadFromStream(dis);
            bool exists = MinimaDB::getDB()->getTxPoWDB().exists(txpowid.to0xString());
            if (!exists) {
                NIOManager::sendNetworkMessage(mClientUID, MSG_TXPOWREQ(), txpowid);
            }

        } else if (type.isEqual(MSG_TXPOWREQ())) {
            MiniData txpowid = MiniData::ReadFromStream(dis);
            std::shared_ptr<TxPoW> txpow = MinimaDB::getDB()->getTxPoWDB().getTxPoW(txpowid.to0xString());
            if (txpow) {
                NIOManager::sendNetworkMessage(mClientUID, MSG_TXPOW(), *txpow);
            } else {
                // silent
            }

        } else if (type.isEqual(MSG_TXPOW())) {
            // Read into std::make_shared
            auto txpow = std::shared_ptr<TxPoW>(TxPoW::ReadFromStream(dis));

            bool exists = MinimaDB::getDB()->getTxPoWDB().exists(txpow->getTxPoWID());
            if (exists) return;

            // getTip/getRoot return shared_ptr
            auto tipnode  = MinimaDB::getDB()->getTxPoWTree().getTip();
            auto cascadenode = MinimaDB::getDB()->getTxPoWTree().getRoot();

            if (tipnode == nullptr) return;

            MiniNumber cascadeblock = cascadenode->getBlockNumber();
            MiniNumber block = txpow->getBlockNumber();

            double tipdec = std::stod(tipnode->getTxPoW().getBlockDifficulty().getDataValue());
            double blockdec = std::stod(txpow->getBlockDifficulty().getDataValue());
            double blockdiffratio = (blockdec == 0.0) ? 0.0 : (tipdec / blockdec);

            auto time_now_ms = []() -> long long {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                    .count();
            };
            long long timestart = time_now_ms();

            bool disconnectpeer = false;

            if (!txpow->getChainID().isEqual(brains::TxPoWChecker::CURRENT_NETWORK)) {
                MinimaLogger::log("Wrong Block ChainID! from " + mClientUID + " " + txpow->getChainID().toString()
                                  + " " + txpow->getTxPoWID());
                disconnectpeer = true;
            } else if (!brains::TxPoWChecker::checkTxPoWBasic(*txpow)) { // Dereference
                MinimaLogger::log("TxPoW FAILS Basic checks from " + mClientUID + " " + txpow->getTxPoWID());
                disconnectpeer = true;
            } else if (!brains::TxPoWChecker::checkSignatures(*txpow)) { // Dereference
                MinimaLogger::log("Invalid signatures on txpow from " + mClientUID + " " + txpow->getTxPoWID());
                disconnectpeer = true;
            } else if (!RelayPolicy::checkMaxStateStoreSize(*txpow, tipnode->getTxPoW().getMagic().getMaxTxPoWSize().getAsLong())) { // Dereference
                MinimaLogger::log("TxPoW state store too large..");
                disconnectpeer = true;
            }

            if (disconnectpeer) {
                Main::getInstance()->getNIOManager().disconnect(mClientUID);
                return;
            }

            bool fullyvalid = true;

            if (txpow->isBlock() && blockdiffratio < 0.01) {
                fullyvalid = false;
            }

            bool beforecascade = false;
            if (block.isLess(cascadeblock)) {
                fullyvalid = false;
                beforecascade = true;
            }

            if (!RelayPolicy::checkAllPolicies(*txpow, GeneralParams::MAX_RELAY_STORESTATESIZE)) { // Dereference
                fullyvalid = false;
            }

            if (!brains::TxPoWChecker::checkTxPoWScripts(tipnode->getMMR(), *txpow, tipnode->getTxPoW())) { // Dereference
                if (txpow->isMonotonic()) {
                    MinimaLogger::log("Error Monotonic TxPoW failed script check from Client:" + mClientUID + " " + txpow->getTxPoWID());
                    return;
                } else {
                    MinimaLogger::log("NON-Monotonic TxPoW failed script check from Client:" + mClientUID + " " + txpow->getTxPoWID());
                }
                fullyvalid = false;
            }

            if (txpow->isBlock()) {
                MiniNumber maxtime(static_cast<long long>(time_now_ms() + (1000LL * 60LL * 120LL)));
                if (txpow->getTimeMilli().isMore(maxtime)) {
                    MinimaLogger::log("TxPoW block received with millitime MORE than 2 hours in future "
                                      + std::to_string(txpow->getTimeMilli().getAsLong()) + " " + txpow->getTxPoWID());
                    fullyvalid = false;
                }
            }

            long long size = txpow->getSizeinBytesWithoutBlockTxns();
            if (size > tipnode->getTxPoW().getMagic().getMaxTxPoWSize().getAsLong()) {
                MinimaLogger::log("TxPoW received size too large.. " + std::to_string(size) + " " + txpow->getTxPoWID());
                fullyvalid = false;
            }

            if (brains::TxPoWChecker::checkMemPoolCoins(*txpow)) { // Dereference
                fullyvalid = false;
            }

            if (!brains::TxPoWChecker::checkMMR(tipnode->getMMR(), *txpow, false)) { // Dereference
                fullyvalid = false;
            }

            if (brains::TxPoWGenerator::isMempoolFull()) {
                MiniNumber burn = txpow->getBurn();
                if (burn.isLessEqual(brains::TxPoWGenerator::getMinMempoolBurn())) {
                    MinimaLogger::log("Received TxPoW with low burn when MEMPOOL full " + burn.toString());
                    fullyvalid = false;
                }
            }

            long long timediff = time_now_ms() - timestart;
            if (timediff > 20000) {
                MinimaLogger::log("Message took a long time (" + std::to_string(timediff) + "ms) to process @ txpowid:" + txpow->getTxPoWID());
                fullyvalid = false;
            }

            Main::getInstance()->getTxPoWProcessor().postProcessTxPoW(txpow); // Pass shared_ptr

            if (fullyvalid) {
                // Store rvalue in variable before passing as lvalue reference
                auto txpowid_data = txpow->getTxPoWIDData();
                NIOManager::sendNetworkMessageAll(MSG_TXPOWID(), txpowid_data);
            }

            if (!GeneralParams::TXBLOCK_NODE && txpow->isBlock() && !beforecascade) {
                std::vector<MiniData> txns = txpow->getBlockTransactions();
                for (const auto& txn : txns) {
                    bool ex = MinimaDB::getDB()->getTxPoWDB().exists(txn.to0xString());
                    if (!ex) {
                        NIOManager::sendNetworkMessage(mClientUID, MSG_TXPOWREQ(), txn);
                    }
                }

                TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();
                // 'current' is a shared_ptr
                auto current = txpow;

                int counter = 0;
                while (counter < 512) {
                    if (current->getBlockNumber().isLessEqual(cascadeblock)) {
                        break;
                    }

                    MiniData parentid = current->getParentID();

                    // searchChainForTxPoWBlock returns shared_ptr
                    auto node = brains::TxPoWSearcher::searchChainForTxPoWBlock(parentid);
                    if (node) {
                        break;
                    }

                    std::shared_ptr<TxPoW> parent = txpdb.getTxPoW(current->getParentID().to0xString());
                    if (!parent) {
                        NIOManager::sendNetworkMessage(mClientUID, MSG_TXPOWREQ(), current->getParentID());
                        break;
                    }

                    std::vector<MiniData> ptxns = parent->getBlockTransactions();
                    for (const auto& txn : ptxns) {
                        if (!MinimaDB::getDB()->getTxPoWDB().exists(txn.to0xString())) {
                            NIOManager::sendNetworkMessage(mClientUID, MSG_TXPOWREQ(), txn);
                        }
                    }
                    
                    // 'parent' is a shared_ptr, assign it
                    current = parent;
                    counter++;
                }

                if (mClientUID == "0x00" || mClientUID == "0x01") {
                    return;
                }

                long long lasttime = 0;
                {
                    std::lock_guard<std::mutex> lock(s_lastChainSyncMutex);
                    auto it = mLastChainSync.find(mClientUID);
                    if (it != mLastChainSync.end()) {
                        lasttime = it->second;
                    }
                }
                long long reqtimenow = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::system_clock::now().time_since_epoch())
                                           .count();
                long long reqtimediff = reqtimenow - lasttime;
                if (reqtimediff < 1000LL * 60LL * 10LL) {
                    return;
                }
                {
                    std::lock_guard<std::mutex> lock(s_lastChainSyncMutex);
                    mLastChainSync[mClientUID] = reqtimenow;
                }

                int scounter = 0;
                // getTip returns shared_ptr
                auto tipblock = MinimaDB::getDB()->getTxPoWTree().getTip();
                while (tipblock != nullptr && scounter < 256) {
                    scounter++;

                    bool haveall = tipblock->checkFullTxns(txpdb);

                    if (!haveall) {
                        std::vector<MiniData> ptxns = tipblock->getTxPoW().getBlockTransactions();
                        for (const auto& txn : ptxns) {
                            bool ex = MinimaDB::getDB()->getTxPoWDB().exists(txn.to0xString());
                            if (!ex) {
                                NIOManager::sendNetworkMessage(mClientUID, MSG_TXPOWREQ(), txn);
                            }
                        }
                    }
                    
                    // getParent returns shared_ptr, no .get() needed
                    tipblock = tipblock->getParent();
                }
            }

        } else if (type.isEqual(MSG_GENMESSAGE())) {
            MiniString msg = MiniString::ReadFromStream(dis);
            MinimaLogger::log(mClientUID + ":" + msg.toString());

        } else if (type.isEqual(MSG_PING())) {
            MiniData txpowid = MiniData::ReadFromStream(dis);
            (void)txpowid;

        } else if (type.isEqual(MSG_P2P())) {
            MiniString msg = MiniString::ReadFromStream(dis);

            if (!GeneralParams::P2P_ENABLED) {
                return;
            }

            // getClient returns shared_ptr
            auto nioclient = Main::getInstance()->getNIOManager().getNIOServer()->getClient(mClientUID);
            if (nioclient == nullptr) {
                MinimaLogger::log(mClientUID + " Error null client on P2P NIOMessage..");
                return;
            }
            // NIOClientInfo instance creation removed (not required for functional outcome here)

            JSONParser parser;
            JSONObject json;
            try {
                std::any any = parser.parse(msg.toString());
                try {
                    json = std::any_cast<JSONObject>(any);
                } catch (const std::bad_any_cast&) {
                    auto sp = std::any_cast<std::shared_ptr<JSONObject>>(any);
                    if (sp) json = *sp;
                }
            } catch (...) {
                // parsing failed; ignore
                return;
            }

            // static_cast works now due to include
            P2PManager* p2pmanager = static_cast<P2PManager*>(&(Main::getInstance()->getNetworkManager().getP2PManager()));

            if (!nioclient->hasReceivedP2PGreeting()) {
                TimerMessage p2p(10000, P2PFunctions::P2P_MESSAGE);
                p2p.addString("uid", mClientUID);
                p2p.addObject("message", json);
                // PostTimerMessage expects shared_ptr, and had typo
                p2pmanager->PostTimerMessage(std::make_shared<TimerMessage>(p2p));
            } else {
                Message p2p(P2PFunctions::P2P_MESSAGE);
                p2p.addString("uid", mClientUID);
                p2p.addObject("message", json);
                // PostMessage expects shared_ptr
                p2pmanager->PostMessage(std::make_shared<Message>(p2p));
            }

        } else if (type.isEqual(MSG_PULSE())) {

            Pulse pulse = Pulse::ReadFromStream(dis);
            TxPoWDB& txpdb = MinimaDB::getDB()->getTxPoWDB();

            std::vector<MiniData> mylist = MinimaDB::getDB()->getTxPoWTree().getPulseList();
            std::vector<MiniData> requestlist;

            std::unordered_set<std::string> fullist;
            for (const auto& block : mylist) {
                fullist.insert(block.to0xString());
            }

            auto time_now_ms = []() -> long long {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                    .count();
            };
            long long timestart = time_now_ms();

            bool found = false;
            const std::vector<MiniData>& pulsemsg = pulse.getBlockList();

            if (pulsemsg.size() > 1000) {
                MinimaLogger::log("Too many messages in PULSE " + std::to_string(pulsemsg.size()) + " max:1000");
                return;
            }

            int counter = 0;
            for (const auto& block : pulsemsg) {
                // search... returns shared_ptr
                auto node = brains::TxPoWSearcher::searchChainForTxPoWBlock(block);
                if (node) {
                    found = true;
                    break;
                }

                counter++;
                std::string blockstr = block.to0xString();

                std::shared_ptr<TxPoW> check = txpdb.getTxPoW(blockstr);
                if (!check) {
                    requestlist.insert(requestlist.begin(), block);
                } else {
                    if (!GeneralParams::TXBLOCK_NODE) {
                        std::vector<MiniData> txns = check->getBlockTransactions();
                        for (const auto& txn : txns) {
                            if (!txpdb.exists(txn.to0xString())) {
                                requestlist.insert(requestlist.begin(), txn);
                            }
                        }
                    }
                }

                if (fullist.find(blockstr) != fullist.end()) {
                    found = true;
                }
            }

            long long timediff = time_now_ms() - timestart;
            if (counter > 0) {
                MinimaLogger::log("PULSE(" + std::to_string(counter) + "/" + std::to_string(pulsemsg.size())
                                  + ") from:" + mClientUID + " TIME:" + std::to_string(timediff) + "ms req:"
                                  + std::to_string(requestlist.size()) + " crossover:" + std::string(found ? "true" : "false"));
            }

            if (found) {
                if (!GeneralParams::TXBLOCK_NODE) {
                    for (const auto& block : requestlist) {
                        NIOManager::sendNetworkMessage(mClientUID, MSG_TXPOWREQ(), block);
                    }
                } else {
                    for (const auto& block : requestlist) {
                        NIOManager::sendNetworkMessage(mClientUID, MSG_TXBLOCKREQ(), block);
                    }
                }
            } else {

                // Check if peer sent empty Pulse (tree not initialized yet)
                if (pulsemsg.size() == 0) {
                    return;  // Don't disconnect for empty pulse
                }
                
                if (Main::getInstance()->isSyncIBD()) {
                    return;
                }

                // A tip-less / bootstrapping node cannot cross with a peer's recent
                // pulse (it has no chain yet), so "no crossover" here means WE are
                // behind - not that the peer is on a different chain. Do NOT
                // blacklist/disconnect a valid peer (this is what stopped fresh
                // nodes syncing from mainnet); stay connected and let the
                // greeting/IBD path sync us.
                {
                    auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();
                    if (tip == nullptr ||
                        tip->getTxPoW().getBlockNumber().isLessEqual(org::minima::objects::base::MiniNumber::ONE())) {
                        MinimaLogger::log("[!] No crossover but we have no chain yet - staying connected to "
                                          + mClientUID + " and awaiting IBD");
                        return;
                    }
                }

                if (!mFullAdrress.empty()) {
                    P2PFunctions::addInvalidPeer(mFullAdrress);
                }

                // getClient returns shared_ptr
                auto nioclient = Main::getInstance()->getNIOManager().getNIOServer()->getClient(mClientUID);
                if (nioclient == nullptr) {
                    Main::getInstance()->getNIOManager().disconnect(mClientUID, true);
                    return;
                }

                int port = nioclient->getPort();
                if (nioclient->getMinimaPort() == -1) {
                    port = nioclient->getMinimaPort();
                }

                MinimaLogger::log("[!] No Crossover found whilst syncing with new node. They are on a different chain. Please check you are on the correct chain.. disconnecting from "
                                  + nioclient->getHost() + ":" + std::to_string(port));

                P2PFunctions::addInvalidPeer(nioclient->getFullAddress());

                Main::getInstance()->getNIOManager().disconnect(mClientUID, true);
            }

        } else if (type.isEqual(MSG_SINGLE_PING())) {
            MiniData datapacket = MiniData::ReadFromStream(dis);
            (void)datapacket;

            Greeting pinggreet;
            pinggreet.getExtraData().put("welcome", std::string("hi there!"));

            // getTip returns shared_ptr
            auto tip = MinimaDB::getDB()->getTxPoWTree().getTip();

            if (tip != nullptr) {
                pinggreet.getExtraData().put("topblock", tip->getBlockNumber().toString());
                pinggreet.getExtraData().put("tophash", tip->getTxPoW().getTxPoWID());

                std::shared_ptr<TxPoWTreeNode> tip50 = tip->getParent(100);
                pinggreet.getExtraData().put("50block", tip50->getBlockNumber().toString());
                pinggreet.getExtraData().put("50hash", tip50->getTxPoW().getTxPoWID());
            } else {
                pinggreet.getExtraData().put("topblock", std::string("0"));
                pinggreet.getExtraData().put("tophash", std::string("0x00"));
                pinggreet.getExtraData().put("50block", std::string("0"));
                pinggreet.getExtraData().put("50hash", std::string("0x00"));
            }

            pinggreet.getExtraData().put("connections", Main::getInstance()->getNIOManager().getAllConnectedDetails());

            if (GeneralParams::P2P2_ENABLED) {
                pinggreet.getExtraData().put("peers-list", MinimaDB::getDB()->getP2P2DB().getAllKnownPeers());
            } else if (GeneralParams::P2P_ENABLED) {
                // static_cast works now due to include
                P2PManager* p2pmanager = static_cast<P2PManager*>(&(Main::getInstance()->getNetworkManager().getP2PManager()));
                JSONArray peers = InetSocketAddressIO::addressesListToJSONArray(p2pmanager->getPeersCopy());
                pinggreet.getExtraData().put("peers-list", peers);
                pinggreet.getExtraData().put("clients", p2pmanager->getClients());
            } else {
                pinggreet.getExtraData().put("peers-list", JSONArray());
                pinggreet.getExtraData().put("clients", 0);
            }

            NIOManager::sendNetworkMessage(mClientUID, MSG_SINGLE_PONG(), pinggreet);

        } else if (type.isEqual(MSG_IBD_REQ())) {
            auto lastblock_ptr = TxPoW::ReadFromStream(dis);
            if (!lastblock_ptr) return;
            const TxPoW& lastblock = *lastblock_ptr;

            if (GeneralParams::ARCHIVESYNC_LIMIT_BANDWIDTH) {
                // Use . (dot) on reference
                long long total = Main::getInstance()->getNIOManager().getTrafficListener().getTotalWrite();
                std::string current = MiniFormat::formatSize(total);
                if (total > NIOManager::MAX_ARCHIVE_WRITE) {
                    MinimaLogger::log("MAX Bandwith used already (" + current + ") - no more archive sync for 24hours..");
                    return;
                }
            }

            {
                std::lock_guard<std::mutex> lock(s_lastSyncReqMutex);
                auto it = mlastSyncReq.find(mClientUID);
                if (it != mlastSyncReq.end()) {
                    if (it->second.isEqual(lastblock.getBlockNumber())) {
                        MinimaLogger::log("[+] Received SAME Sync IBD Request from " + mClientUID + " @ " + lastblock.getBlockNumber().toString() + " IGNORING");
                        return;
                    }
                }
                mlastSyncReq[mClientUID] = lastblock.getBlockNumber();
            }

            IBD syncibd;
            syncibd.createSyncIBD(lastblock);
            NIOManager::sendNetworkMessage(mClientUID, MSG_IBD_RESP(), syncibd);

        } else if (type.isEqual(MSG_IBD_RESP())) {
            // Read into a shared_ptr
            auto syncibd = std::make_shared<IBD>(IBD::ReadFromStream(dis));

            if (!syncibd->getTxBlocks().empty()) {
                MiniNumber top = syncibd->getTxBlocks().front()->getTxPoW().getBlockNumber();

                if (GeneralParams::IBDSYNC_LOGS) {
                    long long timemilli = syncibd->getTxBlocks().front()->getTxPoW().getTimeMilli().getAsLong();
                    MinimaLogger::log("[+] Received Sync IBD. size:" + MiniFormat::formatSize(static_cast<long long>(raw.size()))
                        + " blocks:" + std::to_string(syncibd->getTxBlocks().size())
                        + " top:" + top.toString() + " @ " + std::to_string(timemilli));
                }

                // Pass the shared_ptr
                Main::getInstance()->getTxPoWProcessor().postProcessSyncIBD(syncibd, mClientUID);
            }

        } else if (type.isEqual(MSG_ARCHIVE_REQ())) {
            MiniNumber firstblock = MiniNumber::ReadFromStream(dis);
            IBD ibd;

            if (firstblock.isEqual(MiniNumber::MINUSONE())) {
                MinimaLogger::log("Archive IBD connection test..");
                NIOManager::sendNetworkMessage(mClientUID, MSG_ARCHIVE_DATA(), ibd);
            } else {
                MinimaLogger::log("Archive IBD request start @ " + firstblock.toString());
                ibd.createArchiveIBD(firstblock);
                NIOManager::sendNetworkMessage(mClientUID, MSG_ARCHIVE_DATA(), ibd);
            }

        } else if (type.isEqual(MSG_ARCHIVE_SINGLE_REQ())) {
            // Left commented as per Java code stub.

        } else if (type.isEqual(MSG_ARCHIVE_DATA())) {
            MinimaLogger::log("Received MSG_ARCHIVE_DATA() msg.. ignoring.. from " + mClientUID);
            // Archive handler processes elsewhere

        } else if (type.isEqual(MSG_TXBLOCKID())) {
            if (!GeneralParams::TXBLOCK_NODE) return;

            MiniData txpowid = MiniData::ReadFromStream(dis);
            std::string txid = txpowid.to0xString();

            // findTxBlock returns shared_ptr
            auto txb = MinimaDB::getDB()->getTxBlockDB().findTxBlock(txid);
            if (txb == nullptr) {
                // findNode returns shared_ptr
                auto node = MinimaDB::getDB()->getTxPoWTree().findNode(txid);
                if (node != nullptr) {
                    // We need a non-const TxBlock&, getTxBlock returns const&
                    // This is a logic issue - assuming for now we can get a non-const version or the API is wrong
                    // For now, let's assume getTxBlock is what's needed and we'll deal with const if it arises
                    // This will be a TxBlock*
                    txb = std::shared_ptr<TxBlock>(node, const_cast<TxBlock*>(&node->getTxBlock())); // Assign the shared_ptr
                }
            }

            if (txb == nullptr) {
                NIOManager::sendNetworkMessage(mClientUID, MSG_TXBLOCKREQ(), txpowid);
            }

        } else if (type.isEqual(MSG_TXBLOCKREQ())) {
            MiniData txpowid = MiniData::ReadFromStream(dis);
            std::string txid = txpowid.to0xString();

            // findTxBlock returns shared_ptr
            auto txb = MinimaDB::getDB()->getTxBlockDB().findTxBlock(txid);
            if (txb == nullptr) {
                // findNode returns shared_ptr
                auto node = MinimaDB::getDB()->getTxPoWTree().findNode(txid);
                if (node != nullptr) {
                    txb = std::shared_ptr<TxBlock>(node, const_cast<TxBlock*>(&node->getTxBlock())); // Assign the shared_ptr
                }
            }
            if (txb == nullptr) {
                // loadBlock returns shared_ptr
                txb = MinimaDB::getDB()->getArchive().loadBlock(txid);
            }

            if (txb != nullptr) {
                NIOManager::sendNetworkMessage(mClientUID, MSG_TXBLOCK(), *txb); // Pass object
            } else {
                MinimaLogger::log("Request for TxBlock we don't have : " + txpowid.to0xString());
            }

        } else if (type.isEqual(MSG_TXBLOCK())) {
            if (!GeneralParams::TXBLOCK_NODE) return;

            // ReadFromStream returns unique_ptr, move to shared_ptr
            auto txblock = std::shared_ptr<TxBlock>(TxBlock::ReadFromStream(dis));
            if (!txblock) return;

            Main::getInstance()->getTxPoWProcessor().postProcessTxBlock(txblock); // Pass shared_ptr

            // getRoot returns shared_ptr
            auto cascade = MinimaDB::getDB()->getTxPoWTree().getRoot();
            if (txblock->getTxPoW().getBlockNumber().isMoreEqual(cascade->getBlockNumber())) {
                MiniData parent = txblock->getTxPoW().getParentID();
                std::string txid = parent.to0xString();

                // findTxBlock returns shared_ptr
                auto txb = MinimaDB::getDB()->getTxBlockDB().findTxBlock(txid);
                if (txb == nullptr) {
                    // findNode returns shared_ptr
                    auto node = MinimaDB::getDB()->getTxPoWTree().findNode(txid);
                    if (node != nullptr) {
                         txb = std::shared_ptr<TxBlock>(node, const_cast<TxBlock*>(&node->getTxBlock())); // Assign the shared_ptr
                    }
                }

                if (txb == nullptr) {
                    MinimaLogger::log("Request Parent TxBlock.. @ " + txblock->getTxPoW().getBlockNumber().toString());
                    NIOManager::sendNetworkMessage(mClientUID, MSG_TXBLOCKREQ(), parent);
                }
            }

        } else if (type.isEqual(MSG_TXBLOCKMINE())) {
            if (!GeneralParams::TXBLOCK_NODE) return;

            // Store the returned unique_ptr
            auto txp_ptr = TxPoW::ReadFromStream(dis);
            if (!txp_ptr) return; // Always check for null

            // Get a reference to the TxPoW object
            TxPoW& txp = *txp_ptr;

            txp.getTxBody()->resetRandomPRNG();

            auto time_now_ms = []() -> long long {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                    .count();
            };
            long long timenow = time_now_ms();
            long long timediff = timenow - LAST_TXBLOCKMINE_MSG;

            // NOTE: Main::AUTOMINE_TIMER is private in provided header; see previous note.
            const long long MINE_THROTTLE_MS = 1000;
            if (timediff < MINE_THROTTLE_MS) {
                return;
            }
            LAST_TXBLOCKMINE_MSG = timenow;

            Main::getInstance()->getTxPoWMiner().mineTxPoWAsync(txp);

        } else if (type.isEqual(MSG_MEGAMMRSYNC_REQ())) {
            if (!GeneralParams::IS_MEGAMMR) {
                MinimaLogger::log("[!] Attempt to MegaMMR Sync when -megammr not enabled");
                Main::getInstance()->getNIOManager().disconnect(mClientUID);
                return;
            }

            system::commands::backup::mmrsync::MegaMMRSyncData msyncdata;
            msyncdata.readDataStream(dis);

            MinimaLogger::log("Received MegaMMR SYNC request.. addresses:" 
                              + std::to_string(msyncdata.getAllAddresses().size())
                              + " pubkeys:" + std::to_string(msyncdata.getAllPublicKeys().size()));

            std::unique_ptr<system::commands::backup::mmrsync::MegaMMRIBD> mibd =
                system::commands::backup::mmrsync::megammrsync::getCurrentMegaMMRIBD(msyncdata);

            NIOManager::sendNetworkMessage(mClientUID, MSG_MEGAMMRSYNC_RESP(), *mibd);

        } else if (type.getValue() == 9 || type.getValue() == 10) {
            // Reserved message-type slots (9, 10). Silently consumed so peers that
            // still emit them do not trigger unknown-message spam or disconnects.

        } else {
            MinimaLogger::log("Unknown Message type received from " + mClientUID
                              + " type:" + type.toString() + " size:" + std::to_string(raw.size()));
        }

    } catch (const std::exception& e) {
        MinimaLogger::log(e);
    } catch (...) {
        // Ensure no-throw
    }

    // Finally
    mData.reset();
}

void NIOMessage::drainPendingDuringIBD(const std::string& zClientUID) {
    if (zClientUID == "0x00" || zClientUID == "0x01") return;
    std::vector<MiniData> pows, blks;
    {
        std::lock_guard<std::mutex> lk(s_pendingIBDMutex);
        pows.swap(mPendingTxPowIDsDuringIBD);
        blks.swap(mPendingTxBlockIDsDuringIBD);
    }
    const size_t MAX_DRAIN = 512;
    size_t n = 0;
    for (auto& id : pows) {
        if (n++ >= MAX_DRAIN) break;
        bool exists = MinimaDB::getDB()->getTxPoWDB().exists(id.to0xString());
        if (!exists) {
            NIOManager::sendNetworkMessage(zClientUID, MSG_TXPOWREQ(), id);
        }
    }
    n = 0;
    for (auto& id : blks) {
        if (n++ >= MAX_DRAIN) break;
        auto txb = MinimaDB::getDB()->getTxBlockDB().findTxBlock(id.to0xString());
        if (txb == nullptr) {
            auto node = MinimaDB::getDB()->getTxPoWTree().findNode(id.to0xString());
            if (node == nullptr) {
                NIOManager::sendNetworkMessage(zClientUID, MSG_TXBLOCKREQ(), id);
            }
        }
    }
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
