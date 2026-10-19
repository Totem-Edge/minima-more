#include "org/minima/system/network/p2p/p2_p_manager.hpp"

#include <algorithm>
#include <set>
#include <thread>
#include <chrono>

#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/network/p2p/params/p2_p_test_params.hpp"
#include "org/minima/system/network/p2p/params/p2_p_params.hpp"

#include "org/minima/utils/minima_logger.hpp"

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/system/network/p2p/p2_p_d_b.hpp"

#include "org/minima/system/network/p2p/p2_p_state.hpp"
#include "org/minima/system/network/p2p/p2_p_peers_checker.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"
#include "org/minima/system/network/p2p/swap_links_functions.hpp"
#include "org/minima/system/network/p2p/walk_links_funcs.hpp"

#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/system/network/p2p/messages/p2_p_greeting.hpp"
#include "org/minima/system/network/p2p/messages/p2_p_do_swap.hpp"
#include "org/minima/system/network/p2p/messages/p2_p_walk_links.hpp"

#include "org/minima/system/main.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/minima/n_i_o_server.hpp"
#include "org/minima/system/network/minima/n_i_o_client.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"

#include "org/minima/system/commands/network/connect.hpp"
#include "org/minima/objects/greeting.hpp"

#include "org/minima/utils/messages/timer_message.hpp"
#include "org/minima/utils/messages/message_stack.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

using org::minima::utils::MinimaLogger;
using org::minima::utils::messages::Message;
using org::minima::utils::messages::TimerMessage;
using org::minima::system::params::GeneralParams;
using org::minima::system::network::p2p::params::P2PParams;
using org::minima::system::network::p2p::params::P2PTestParams;
using org::minima::system::network::minima::NIOManager;
using org::minima::system::network::minima::NIOClient;
using org::minima::system::network::minima::NIOClientInfo;
using org::minima::utils::json::JSONObject;
using org::minima::utils::json::JSONArray;
//
// FIX 1: Remove the conflicting 'using' directive
// using org::minima::system::network::p2p::messages::InetSocketAddress; // REMOVED
//
using org::minima::system::network::p2p::messages::InetSocketAddressIO;
using org::minima::system::network::p2p::messages::P2PGreeting;
using org::minima::system::network::p2p::messages::P2PWalkLinks;
using org::minima::system::network::p2p::messages::P2PDoSwap;

//
// FIX 2: Explicitly type the parameter to the correct 'messages' version
//
static inline std::string hostStringFrom(const org::minima::system::network::p2p::messages::InetSocketAddress& addr) {
    const std::string& orig = addr.originalHost();
    if (!orig.empty()) return orig;
    return addr.getAddress().getHostAddress();
}

P2PManager::P2PManager()
    : MessageProcessor("P2PMANAGER")
    , mState(std::make_unique<P2PState>())
    , mRng(std::random_device{}()) {

    if (GeneralParams::TEST_PARAMS) {
        MinimaLogger::log("[+] P2P System using Test Params");
        P2PTestParams::setTestParams();
    }

    // Start the peers checker
    mPeersChecker = std::make_unique<P2PPeersChecker>(this);

    // Timed loop messages
    PostTimerMessage(std::make_shared<TimerMessage>(10'000, P2P_LOOP));
    PostTimerMessage(std::make_shared<TimerMessage>(P2PParams::NODE_NOT_ACCEPTING_CHECK_DELAY, P2P_ASSESS_CONNECTIVITY));
    PostTimerMessage(std::make_shared<TimerMessage>(static_cast<std::int64_t>(1000) * 60 * 5, P2P_SAVE_DATA));

    // Excluded when we clear the list
    mExcludeFromClear.push_back(P2P_LOOP);
    mExcludeFromClear.push_back(P2P_ASSESS_CONNECTIVITY);
    mExcludeFromClear.push_back(P2P_SAVE_DATA);

    startMessageProcessorThread();
}

P2PManager::~P2PManager() = default;

P2PPeersChecker* P2PManager::getPeersChecker() {
    return mPeersChecker.get();
}

//
// FIX 3: Use the fully qualified type to match the header
//
std::vector<org::minima::system::network::p2p::messages::InetSocketAddress> P2PManager::getPeersCopy() {
    return mState->getKnownPeersCopy();
}

std::string P2PManager::getP2PAddress() {
    const auto& myaddr = mState->getMyMinimaAddress();
    //
    // FIX 4: hostStringFrom fix (FIX 2) resolves the error here
    //
    std::string host = hostStringFrom(myaddr);
    return host + ":" + std::to_string(myaddr.getPort());
}

float P2PManager::getClients() {
    float denom = static_cast<float>(P2PParams::MIN_NUM_CONNECTIONS);
    if (denom <= 0.0f) {
        return 0.0f;
    }
    return static_cast<float>(mState->getNoneP2PLinks().size()) / denom;
}

std::vector<Message> P2PManager::init(P2PState& state) {
    std::vector<Message> msgs;

    //
    // FIX 5: getP2PDB() returns a reference, not a pointer. Use auto&
    //
    auto& p2pdb = org::minima::database::MinimaDB::getDB()->getP2PDB();
    std::string p2pVersion = p2pdb.getVersion();

    MinimaLogger::log(std::string("[+] P2P Version: ") + P2PParams::VERSION);

    // Load peers list
    std::vector<InetSocketAddress> peers = p2pdb.getPeersList();
    mInitialPeersListNum = static_cast<int>(peers.size());
    MinimaLogger::log(std::string("P2P Peers found : ") + std::to_string(mInitialPeersListNum));

    // Do we add any nodes specified in params only when the peers list is empty
    if (!GeneralParams::P2P_ADDNODES.empty() && peers.empty()) {
        MinimaLogger::log(std::string("Peers list empty - adding specified p2pnodes.. ") +
            GeneralParams::P2P_ADDNODES);

        // Tokenize by comma
        std::string addnodes = GeneralParams::P2P_ADDNODES;
        std::size_t start = 0;
        while (start != std::string::npos) {
            std::size_t comma = addnodes.find(',', start);
            std::string node = (comma == std::string::npos) ? addnodes.substr(start) : addnodes.substr(start, comma - start);
            // trim
            node.erase(0, node.find_first_not_of(" \t\r\n"));
            if (!node.empty()) node.erase(node.find_last_not_of(" \t\r\n") + 1);

            auto checker = org::minima::system::commands::network::connect::createConnectMessage(node);
            if (!checker) {
                MinimaLogger::log(std::string("Invalid P2P node : ") + node);
            } else {
                std::string host = checker->getString("host");
                int port = checker->getInteger("port");
                InetSocketAddress addr(host, port);
                peers.push_back(addr);
            }

            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }

    // Cycle our known peers - send to checker
    for (const auto& peer : peers) {
        if (mPeersChecker) {
            auto addpeer = std::make_shared<Message>(P2PPeersChecker::PEERS_CHECKPEERS);
            addpeer->addObject("address", peer).addBoolean("force", true);
            mPeersChecker->PostMessage(addpeer);
        }
    }

    state.setAcceptingInLinks(GeneralParams::IS_ACCEPTING_IN_LINKS);
    state.setMyMinimaAddress(GeneralParams::MINIMA_HOST);

    if (GeneralParams::IS_HOST_SET) {
        state.setHostSet(true);
    }

    state.setNoConnect(GeneralParams::NOCONNECT);

    // Initialise limits
    state.setMaxNumNoneP2PConnections(P2PParams::TGT_NUM_NONE_P2P_LINKS);
    if (state.isAcceptingInLinks()) {
        state.setMaxNumP2PConnections(P2PParams::TGT_NUM_LINKS);
    } else {
        state.setMaxNumP2PConnections(P2PParams::MIN_NUM_CONNECTIONS);
    }

    std::unique_ptr<InetSocketAddress> connectionAddress;

    if (!state.isNoConnect()) {
        if (!GeneralParams::P2P_ROOTNODE.empty()) {
            auto pos = GeneralParams::P2P_ROOTNODE.find(':');
            if (pos != std::string::npos) {
                std::string host = GeneralParams::P2P_ROOTNODE.substr(0, pos);
                int port = std::stoi(GeneralParams::P2P_ROOTNODE.substr(pos + 1));
                connectionAddress = std::make_unique<InetSocketAddress>(host, port);
                if (mPeersChecker) {
                    auto addpeer = std::make_shared<Message>(P2PPeersChecker::PEERS_ADDPEERS);
                    addpeer->addObject("address", *connectionAddress);
                    mPeersChecker->PostMessage(addpeer);
                }
                P2PFunctions::log_info(std::string("[+] Connecting to specified node: ") + host + ":" + std::to_string(port));
            }
        } else if (!peers.empty()) {
            std::uniform_int_distribution<std::size_t> dist(0, peers.size() - 1);
            const auto& pick = peers[dist(mRng)];
            connectionAddress = std::make_unique<InetSocketAddress>(pick);
            //
            // FIX 4: hostStringFrom fix (FIX 2) resolves the error here
            //
            P2PFunctions::log_info(std::string("[+] Connecting to saved node: ") + hostStringFrom(*connectionAddress) + ":" + std::to_string(connectionAddress->getPort()));
        } else {
            // Discovery attempt
            doDiscoveryPing();
        }
    }

    if (connectionAddress) {
        Message mm(P2PManager::P2P_SEND_CONNECT);
        mm.addObject(ADDRESS_LITERAL, *connectionAddress);
        msgs.push_back(mm);
    }

    return msgs;
}

void P2PManager::doDiscoveryPing() {
    // Discovery sources: the compiled-in seed list, otherwise the peers we are
    // already connected to. Using connected peers lets a node started with only
    // -p2pnodes learn the wider network instead of staying on one connection
    // (and it feeds the peer's advertised "peers-list" to the PeersChecker).
    auto seeds = P2PParams::DEFAULT_NODE_LIST;

    if (seeds.empty() && !org::minima::system::params::GeneralParams::P2P_ADDNODES.empty()) {
        // Fall back to the configured -p2pnodes, skipping any peer we already hold
        // a connection to (a discovery ping to such a peer is refused - it only
        // accepts one link). Pinging our OTHER configured peers lets the node learn
        // the wider network and fan out beyond the single bootstrap connection.
        std::set<std::string> connected;
        try {
            auto* main = org::minima::system::Main::getInstance();
            if (main != nullptr) {
                for (const auto& c : main->getNIOManager().getNIOServer()->getAllNIOClients()) {
                    if (c) connected.insert(c->getHost() + ":" + std::to_string(c->getPort()));
                }
            }
        } catch (...) {
            // best effort
        }

        const std::string& addnodes = org::minima::system::params::GeneralParams::P2P_ADDNODES;
        std::size_t start = 0;
        while (start < addnodes.size()) {
            std::size_t comma = addnodes.find(',', start);
            std::string node = (comma == std::string::npos) ? addnodes.substr(start)
                                                            : addnodes.substr(start, comma - start);
            start = (comma == std::string::npos) ? addnodes.size() : comma + 1;
            // trim
            std::size_t b = node.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) continue;
            std::size_t e = node.find_last_not_of(" \t\r\n");
            node = node.substr(b, e - b + 1);
            if (node.empty() || connected.count(node)) continue;
            std::size_t colon = node.rfind(':');
            if (colon == std::string::npos) continue;
            try {
                seeds.emplace_back(node.substr(0, colon), std::stoi(node.substr(colon + 1)));
            } catch (...) {
                // skip malformed entry
            }
        }
    }

    if (seeds.empty()) {
        long timenow = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        if (timenow - mLastNotifyNoPeers > mNotifyNoPeersTimer) {
            MinimaLogger::log("No peers to discover from yet - connect to a peer first..");
            mLastNotifyNoPeers = timenow;
        }
        return;
    }

    // The loop exits immediately unless discovery is flagged active.
    mState->setDoingDiscoveryConnection(true);

    int attempts = 0;
    while (mState->isDoingDiscoveryConnection() && isRunning()) {
        if (attempts >= 3) {
            P2PFunctions::log_debug(std::string("Discovery node connection paused.. tried ") + std::to_string(attempts) + " times..");
            mState->setDoingDiscoveryConnection(false);
            return;
        }

        // Pick a random discovery source
        std::uniform_int_distribution<std::size_t> dist(0, seeds.size() - 1);
        const auto& address = seeds[dist(mRng)];

        auto greet = NIOManager::sendPingMessage(address.host, address.port, true);
        if (greet) {
            JSONObject& extra = greet->getExtraData();
            if (extra.containsKey("peers-list")) {
                try {
                    const std::any& v = extra.get("peers-list");
                    const JSONArray& peersArrayList = std::any_cast<const JSONArray&>(v);
                    auto peers = InetSocketAddressIO::addressesJSONArrayToList(peersArrayList);
                    std::shuffle(peers.begin(), peers.end(), mRng);

                    // Insert range into known set
                    mState->getKnownPeers().insert(peers.begin(), peers.end());

                    // Add discovered peers to the verified list so they are used immediately
                    if (mPeersChecker) {
                        mPeersChecker->getVerifiedPeers().insert(peers.begin(), peers.end());
                    }

                    P2PFunctions::log_info("[+] Discovery Completed - learned " + std::to_string(peers.size()) + " peers");
                    mState->setDoingDiscoveryConnection(false);
                } catch (const std::bad_any_cast&) {
                    // Ignore malformed extra data
                }
            }
        } else {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            } catch (...) {
                P2PFunctions::log_debug("Wait interrupted");
            }
        }

        attempts++;
    }
}

void P2PManager::processMessage(Message& zMessage) {
    std::vector<Message> sendMsgs;

    if (zMessage.isMessageType(P2PFunctions::P2P_INIT)) {
        auto initmsgs = init(*mState);
        sendMsgs.insert(sendMsgs.end(), initmsgs.begin(), initmsgs.end());

    } else if (zMessage.isMessageType(P2PFunctions::P2P_SHUTDOWN)) {
        shutdown();

    } else if (zMessage.isMessageType(P2P_HEALTH_CHECK)) {
        int count = getSize();
        if (count > 50) {
            MinimaLogger::log("[!] P2P Message Overload - clear non-timer messages");
            clearExcept(mExcludeFromClear);
        }
        PostTimerMessage(std::make_shared<TimerMessage>(mP2PHealthCheckTimer, P2P_HEALTH_CHECK));

    } else if (zMessage.isMessageType(P2P_SAVE_DATA)) {
        updateP2PPeersList();
        org::minima::database::MinimaDB::getDB()->saveP2PDB();
        PostTimerMessage(std::make_shared<TimerMessage>(P2PParams::SAVE_DATA_DELAY, P2P_SAVE_DATA));

    } else if (zMessage.isMessageType(P2PFunctions::P2P_CONNECTED)) {
        std::string uid = zMessage.getString("uid");
        auto anycli = zMessage.getObject("client");
        NIOClient* client = nullptr;
        try {
            client = std::any_cast<NIOClient*>(anycli);
        } catch (...) {
            client = nullptr;
        }
        if (client) {
            mState->getAllLinks()[uid] = InetSocketAddress(client->getHost(), client->getPort());
            auto msgs2 = connect(zMessage, *mState);
            sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());
        }

    } else if (zMessage.isMessageType(P2PFunctions::P2P_DISCONNECTED)) {
        std::string uid = zMessage.getString("uid");
        mState->getAllLinks().erase(uid);
        SwapLinksFunctions::onDisconnected(*mState, zMessage);
        if (mState->getOutLinks().count(uid)) {
            P2PFunctions::log_debug(std::string("[-] P2P_DISCONNECTED from: ") + uid + " Current outlinks: " + std::to_string(mState->getOutLinks().size()));
        }

    } else if (zMessage.isMessageType(P2PFunctions::P2P_MESSAGE)) {
        auto msgs2 = processJsonMessages(zMessage, *mState);
        sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());

    } else if (zMessage.isMessageType(P2P_LOOP)) {
        auto msgs2 = processLoop(*mState);
        sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());
        PostTimerMessage(std::make_shared<TimerMessage>(mState->getLoopDelay(), P2P_LOOP));

    } else if (zMessage.isMessageType(P2P_RESET)) {
        P2PFunctions::log_debug("[+] P2P Reset in process");
        mState->setAcceptingInLinks(GeneralParams::IS_ACCEPTING_IN_LINKS);
        mState->setMyMinimaAddress(GeneralParams::MINIMA_HOST);
        mState->setHostSet(GeneralParams::IS_HOST_SET);
        auto msgs2 = processLoop(*mState);
        sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());

    } else if (zMessage.isMessageType(P2P_RANDOM_CONNECT)) {
        auto& known = mState->getKnownPeers();
        if (!known.empty()) {
            std::uniform_int_distribution<std::size_t> dist(0, known.size() - 1);
            auto it = known.begin();
            std::advance(it, dist(mRng));
            InetSocketAddress connectionAddress = *it;
            Message mm(P2PManager::P2P_SEND_CONNECT);
            mm.addObject(ADDRESS_LITERAL, connectionAddress);
            sendMsgs.push_back(mm);
        }

    } else if (zMessage.isMessageType(P2PFunctions::P2P_NOCONNECT)) {
        auto anycli = zMessage.getObject("client");
        NIOClient* client = nullptr;
        try {
            client = std::any_cast<NIOClient*>(anycli);
        } catch (...) {
            client = nullptr;
        }
        if (client) {
            InetSocketAddress conn(client->getHost(), client->getPort());
            auto& known = mState->getKnownPeers();
            known.erase(conn);

            P2PFunctions::log_debug("[-] Unable to connect to peer removing from peers list");
            std::vector<std::string> uidsToRemove;
            auto& inlinks = mState->getInLinks();
            for (const auto& kv : inlinks) {
                if (inetAddrEqual(kv.second, conn)) {
                    uidsToRemove.push_back(kv.first);
                }
            }
            for (const auto& uid2 : uidsToRemove) {
                mState->getInLinks().erase(uid2);
                mState->getNotAcceptingConnP2PLinks()[uid2] = mState->getAllLinks()[uid2];
            }
        }

    } else if (zMessage.isMessageType(P2P_ASSESS_CONNECTIVITY)) {
        auto msgs2 = assessConnectivity(*mState);
        sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());
        PostTimerMessage(std::make_shared<TimerMessage>(P2PParams::NODE_NOT_ACCEPTING_CHECK_DELAY, P2P_ASSESS_CONNECTIVITY));

    } else if (zMessage.isMessageType(P2P_REMOVE_PEER)) {
        const auto& anyaddr = zMessage.getObject("address");
        InetSocketAddress address = std::any_cast<InetSocketAddress>(anyaddr);
        auto& known = mState->getKnownPeers();
        known.erase(address);

    } else if (zMessage.isMessageType(P2P_ADD_PEER)) {
        auto& known = mState->getKnownPeers();
        // If limit available, enforce
        if (known.size() < static_cast<std::size_t>(P2PPeersChecker::MAX_VERIFIED_PEERS)) {
            const auto& anyaddr = zMessage.getObject("address");
            InetSocketAddress address = std::any_cast<InetSocketAddress>(anyaddr);
            known.erase(address);
            known.insert(address);
        }
    }

    // Now send out the queued network actions
    try {
        sendMessages(sendMsgs);
    } catch (const std::exception& exc) {
        org::minima::utils::MinimaLogger::log("P2PManager sendMessages exception: " + std::string(exc.what()));
    } catch (...) {
        org::minima::utils::MinimaLogger::log("P2PManager sendMessages unknown exception");
    }
}

std::vector<Message> P2PManager::assessConnectivity(P2PState& state) {
    std::vector<Message> sendmsgs;
    if (state.getInLinks().empty() &&
        state.getNotAcceptingConnP2PLinks().empty() &&
        state.getNoneP2PLinks().empty() &&
        !state.getOutLinks().empty()) {

        state.setAcceptingInLinks(false);
        JSONObject notAcceptingMsg;
        notAcceptingMsg.put("notAcceptingMsg", false);
        state.setMaxNumP2PConnections(P2PParams::MIN_NUM_CONNECTIONS);
        Message mm(P2PManager::P2P_SEND_MSG_TO_ALL);
        mm.addObject("json", notAcceptingMsg);
        sendmsgs.push_back(mm);
    }
    return sendmsgs;
}

std::vector<Message> P2PManager::processJsonMessages(Message& zMessage, P2PState& state) {
    std::vector<Message> sendMsgs;

    // Get the message payload
    JSONObject message = std::any_cast<JSONObject>(zMessage.getObject("message"));

    // Get the UID and current client info
    std::string uid = zMessage.getString("uid");
    //
    // FIX 6: getNIOCLientInfo returns std::unique_ptr, not a raw pointer
    //
    auto client = P2PFunctions::getNIOCLientInfo(uid);
    if (client == nullptr) {
        P2PFunctions::log_debug(std::string("[!] P2P NULL NioClient @ ") + uid);
        Message mm(P2P_SEND_DISCONNECT);
        mm.addString("uid", uid);
        sendMsgs.push_back(mm);
        return sendMsgs;
    }

    // swap_links_p2p
    if (message.containsKey("swap_links_p2p")) {
        JSONObject swapLinksMsg = std::any_cast<JSONObject>(message.get("swap_links_p2p"));

        if (swapLinksMsg.containsKey("greeting")) {
            P2PGreeting greeting = P2PGreeting::fromJSON(std::any_cast<JSONObject>(swapLinksMsg.get("greeting")));
            for (const auto& address : greeting.getKnownPeers()) {
                if (mPeersChecker) {
                    auto addpeer = std::make_shared<Message>(P2PPeersChecker::PEERS_ADDPEERS);
                    addpeer->addObject("address", address);
                    mPeersChecker->PostMessage(addpeer);
                }
            }
            InetSocketAddress minimaAddress(client->getHost(), greeting.getMyMinimaPort());
            if (mPeersChecker) {
                auto addpeer2 = std::make_shared<Message>(P2PPeersChecker::PEERS_ADDPEERS);
                addpeer2->addObject("address", minimaAddress);
                mPeersChecker->PostMessage(addpeer2);
            }

            //
            // FIX 6: Pass the unique_ptr's raw pointer
            //
            bool noConnect = SwapLinksFunctions::processGreeting(state, greeting, client.get(), state.isNoConnect());
            if (!noConnect) {
                state.setNoConnect(false);
            }
        }
        if (swapLinksMsg.containsKey("req_ip")) {
            P2PFunctions::sendP2PMessage(uid, SwapLinksFunctions::processRequestIPMsg(swapLinksMsg, client->getHost()));
        }
        if (swapLinksMsg.containsKey("res_ip")) {
            SwapLinksFunctions::processResponseIPMsg(state, swapLinksMsg);
        }
        if (swapLinksMsg.containsKey("notAcceptingMsg")) {
            state.getInLinks().erase(uid);
            state.getOutLinks().erase(uid);
            state.getNotAcceptingConnP2PLinks().erase(uid);
            state.getNoneP2PLinks().erase(uid);

            state.getNotAcceptingConnP2PLinks()[uid] = state.getAllLinks()[uid];
        }
        if (swapLinksMsg.containsKey("walk_links")) {
            //
            // FIX 6: Dereference the unique_ptr to pass a reference
            //
            auto msgs2 = processWalkLinksMsg(swapLinksMsg, *client, state);
            sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());
        }
        if (swapLinksMsg.containsKey("do_swap")) {
            // Execute Do Swap
            P2PDoSwap doSwap = P2PDoSwap::readFromJson(swapLinksMsg);
            if (const auto* target = doSwap.getSwapTarget()) {
                // getSwapTarget returns the nested P2PDoSwap::InetSocketAddress;
                // build the equivalent messages::InetSocketAddress from host/port.
                InetSocketAddress addr(target->getHostString(), target->getPort());
                Message mm(P2P_SEND_CONNECT);
                mm.addObject(ADDRESS_LITERAL, addr);
                sendMsgs.push_back(mm);
            }
            sendMsgs.push_back(Message(P2P_SEND_DISCONNECT).addString("uid", uid));
        }
    }

    return sendMsgs;
}

std::vector<Message> P2PManager::processLoop(P2PState& state) {
    std::vector<Message> sendMsgs;

    if (state.getOutLinks().size() >= static_cast<std::size_t>(state.getMaxNumP2PConnections())) {
        std::uniform_int_distribution<int> dist(0, P2PParams::LOOP_DELAY_VARIABILITY > 0 ? P2PParams::LOOP_DELAY_VARIABILITY - 1 : 0);
        state.setLoopDelay(P2PParams::LOOP_DELAY + static_cast<long>(dist(mRng)));
    } else {
        std::uniform_int_distribution<int> dist(0, 3000);
        state.setLoopDelay(10'000 + static_cast<long>(dist(mRng)));
    }

    // Remove our own address from known peers
    {
        auto& kp = state.getKnownPeers();
        const auto& myaddr = state.getMyMinimaAddress();
        kp.erase(myaddr);
    }

    if (!state.isNoConnect()) {
        auto& known = state.getKnownPeers();

        // On startup, seed the known list from the persisted P2P DB so user-added
        // peers are not lost between runs and can be re-tried immediately.
        if (known.empty() && !state.isStartupComplete()) {
            auto& p2pdb = org::minima::database::MinimaDB::getDB()->getP2PDB();
            auto peers = p2pdb.getPeersList();
            known.insert(peers.begin(), peers.end());
            for (const auto& peer : peers) {
                if (mPeersChecker) {
                    auto addpeer = std::make_shared<Message>(P2PPeersChecker::PEERS_ADDPEERS);
                    addpeer->addObject("address", peer);
                    mPeersChecker->PostMessage(addpeer);
                }
            }
        }

        if (!known.empty()) {
            if (state.getOutLinks().size() < 1) {
                std::uniform_int_distribution<std::size_t> dist(0, known.size() - 1);
                auto it = known.begin();
                std::advance(it, dist(mRng));
                InetSocketAddress connectionAddress = *it;
                sendMsgs.push_back(Message(P2PManager::P2P_SEND_CONNECT).addObject(ADDRESS_LITERAL, connectionAddress));
            } else if (state.getOutLinks().size() < static_cast<std::size_t>(state.getMaxNumP2PConnections())) {
                // Copy unique_ptr list to value list
                auto uniq = P2PFunctions::getAllConnections();
                std::vector<NIOClientInfo> clients;
                clients.reserve(uniq.size());
                for (auto& p : uniq) {
                    if (p) clients.push_back(*p);
                }
                auto msgs2 = SwapLinksFunctions::joinScaleOutLinks(state, state.getMaxNumP2PConnections(), clients);
                sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());
            } else if (state.isAcceptingInLinks()) {
                auto uniq = P2PFunctions::getAllConnections();
                std::vector<NIOClientInfo> clients;
                clients.reserve(uniq.size());
                for (auto& p : uniq) {
                    if (p) clients.push_back(*p);
                }
                auto msgs2 = SwapLinksFunctions::requestInLinks(state, state.getMaxNumP2PConnections(), clients);
                sendMsgs.insert(sendMsgs.end(), msgs2.begin(), msgs2.end());
                auto msgs3 = SwapLinksFunctions::onConnectedLoadBalanceRequest(state, clients);
                sendMsgs.insert(sendMsgs.end(), msgs3.begin(), msgs3.end());
            }
        } else {
            if (state.getAllLinks().empty() && state.isStartupComplete()) {
                P2PFunctions::log_node_runner("[!] Node is not connected to the network. Attempting to join the network again now. Please check your node has an internet connection.");
            }
            // Do discovery ping
            doDiscoveryPing();
        }
    }

    return sendMsgs;
}

std::vector<Message> P2PManager::processWalkLinksMsg(JSONObject& zMessage, NIOClientInfo& clientInfo, P2PState& state) {
    P2PWalkLinks p2pWalkLinks = P2PWalkLinks::readFromJSON(zMessage);
    std::vector<Message> sendMsg;
    if (p2pWalkLinks.isReturning()) {
        auto msgs = processReturningMessage(p2pWalkLinks, state);
        sendMsg.insert(sendMsg.end(), msgs.begin(), msgs.end());
    } else {
        sendMsg.push_back(processOutgoingWalkMessage(p2pWalkLinks, clientInfo, state));
    }
    return sendMsg;
}

std::vector<Message> P2PManager::processReturningMessage(P2PWalkLinks p2pWalkLinks, P2PState& state) {
    std::vector<Message> msgs;
    if (inetAddrEqual(state.getMyMinimaAddress(), p2pWalkLinks.getPathTaken().front())) {
        if (p2pWalkLinks.isClientWalk()) {
            auto ret = WalkLinksFuncs::onReturnedLoadBalanceWalkMsg(state, p2pWalkLinks);
            for (auto& up : ret) {
                msgs.push_back(up); // MODIFIED
            }
        } else {
            auto ret = WalkLinksFuncs::onReturnedWalkMsg(state, p2pWalkLinks, state.getMaxNumP2PConnections());
            for (auto& up : ret) {
                msgs.push_back(up); // MODIFIED
            }
        }
    } else {
        auto up = WalkLinksFuncs::onWalkLinkResponseMsg(state, p2pWalkLinks);
        if (up) msgs.push_back(*up);
    }
    return msgs;
}

Message P2PManager::processOutgoingWalkMessage(P2PWalkLinks p2pWalkLinks, NIOClientInfo& clientInfo, P2PState& state) {
    // Build vector of objects from pointers
    auto uniq = P2PFunctions::getAllConnections();
    std::vector<NIOClientInfo> clients; // MODIFIED
    clients.reserve(uniq.size()); // MODIFIED
    //
    // FIX 7: Iterate over references to unique_ptr, not raw pointers
    //
    for (auto& p : uniq) { // MODIFIED
        if (p) clients.push_back(*p); // MODIFIED
    }

    if (p2pWalkLinks.isWalkInLinks()) {
        auto up = WalkLinksFuncs::onInLinkWalkMsg(state, p2pWalkLinks, clientInfo, clients); // MODIFIED
        if (up) return *up;
        return Message(); // empty message type -> ignored
    } else {
        auto up = WalkLinksFuncs::onOutLinkWalkMsg(state, p2pWalkLinks, clientInfo, state.getMaxNumP2PConnections(), clients); // MODIFIED
        if (up) return *up;
        return Message(); // empty message type -> ignored
    }
}

std::vector<Message> P2PManager::connect(Message& zMessage, P2PState& state) {
    auto anycli = zMessage.getObject("client");
    NIOClient* info = nullptr;
    try {
        info = std::any_cast<NIOClient*>(anycli);
    } catch (...) {
        info = nullptr;
    }
    std::vector<Message> msgs;
    if (info) {
        auto ret = SwapLinksFunctions::onConnected(state, info->isIncoming(), *info);
        msgs.insert(msgs.end(), ret.begin(), ret.end());

        auto uniq = P2PFunctions::getAllConnections();
        std::vector<NIOClientInfo> clients;
        clients.reserve(uniq.size());
        for (auto& p : uniq) {
            if (p) clients.push_back(*p);
        }
        auto ret2 = SwapLinksFunctions::onConnectedLoadBalanceRequest(state, clients);
        msgs.insert(msgs.end(), ret2.begin(), ret2.end());
    }
    return msgs;
}

JSONObject P2PManager::getStatus(bool fullDetails) {
    int numInbound = 0;
    int numOutbound = 0;

    auto connections = P2PFunctions::getAllConnections();
    for (const auto& infoPtr : connections) {
        if (infoPtr && infoPtr->isConnected()) {
            if (infoPtr->isIncoming()) {
                numInbound += 1;
            } else {
                numOutbound += 1;
            }
        }
    }

    JSONObject ret;
    if (fullDetails) {
        ret.put("p2p_state", mState->toJson());
        std::size_t unvalidated = 0;
        if (mPeersChecker) {
            unvalidated = mPeersChecker->getUnverifiedPeers().size();
        }
        ret.put("numUnvalidatedPeers", static_cast<int>(unvalidated));
    } else {
        const auto& myaddr = mState->getMyMinimaAddress();
        //
        // FIX 4: hostStringFrom fix (FIX 2) resolves the error here
        //
        std::string addr = hostStringFrom(myaddr) + ":" + std::to_string(myaddr.getPort());

        ret.put("address", addr);
        ret.put("isAcceptingInLinks", mState->isAcceptingInLinks());
        ret.put("numInLinks", static_cast<int>(mState->getInLinks().size()));
        ret.put("numOutLinks", static_cast<int>(mState->getOutLinks().size()));
        ret.put("numNotAcceptingConnP2PLinks", static_cast<int>(mState->getNotAcceptingConnP2PLinks().size()));
        ret.put("numNoneP2PLinks", static_cast<int>(mState->getNoneP2PLinks().size()));
        ret.put("numKnownPeers", static_cast<int>(mState->getKnownPeers().size()));
        std::size_t unvalidated = 0;
        if (mPeersChecker) {
            unvalidated = mPeersChecker->getUnverifiedPeers().size();
        }
        ret.put("numUnvalidatedPeers", static_cast<int>(unvalidated));
        ret.put("numAllLinks", static_cast<int>(mState->getAllLinks().size()));
        ret.put("nio_inbound", numInbound);
        ret.put("nio_outbound", numOutbound);
    }

    return ret;
}

void P2PManager::sendMessages(const std::vector<Message>& sendMessages) {
    if (sendMessages.empty()) {
        return;
    }

    for (const auto& msg : sendMessages) {
        if (msg.getMessageType().empty()) continue;

        if (msg.isMessageType(P2P_SEND_CONNECT)) {
            InetSocketAddress address = std::any_cast<InetSocketAddress>(msg.getObject(ADDRESS_LITERAL));
            //
            // FIX 4: hostStringFrom fix (FIX 2) resolves the error here
            //
            P2PFunctions::checkConnect(hostStringFrom(address), address.getPort());

        } else if (msg.isMessageType(P2P_SEND_DISCONNECT)) {
            std::string uid = msg.getString("uid");
            P2PFunctions::disconnect(uid);

        } else if (msg.isMessageType(P2P_SEND_MSG)) {
            std::string uid = msg.getString("uid");
            JSONObject json = std::any_cast<JSONObject>(msg.getObject("json"));
            P2PFunctions::sendP2PMessage(uid, SwapLinksFunctions::wrapP2PMsg(json));

        } else if (msg.isMessageType(P2P_SEND_MSG_TO_ALL)) {
            JSONObject json = std::any_cast<JSONObject>(msg.getObject("json"));
            P2PFunctions::sendP2PMessageAll(SwapLinksFunctions::wrapP2PMsg(json));
        }
    }
}

void P2PManager::updateP2PPeersList() {
    //
    // FIX 5: getP2PDB() returns a reference, not a pointer. Use auto&
    //
    auto& p2pdb = org::minima::database::MinimaDB::getDB()->getP2PDB();

    // Persist whatever known peers we have, even a single user-added peer.
    // The old threshold (>= initialPeersListNum / 2) meant user-added peers
    // were never saved until a large random set was discovered, so a node
    // that lost its connection had no fallback.
    std::vector<InetSocketAddress> vec;
    vec.reserve(mState->getKnownPeers().size());
    for (const auto& it : mState->getKnownPeers()) {
        vec.push_back(it);
    }
    p2pdb.setPeersList(vec);
}

void P2PManager::shutdown() {
    // Write stuff to P2P DB
    //
    // FIX 5: getP2PDB() returns a reference, not a pointer. Use auto&
    //
    auto& p2pdb = org::minima::database::MinimaDB::getDB()->getP2PDB();
    p2pdb.setVersion();

    // Update the peers list
    updateP2PPeersList();

    // Stop the peers checker
    if (mPeersChecker) {
        mPeersChecker->stopMessageProcessor();
    }

    MinimaLogger::log("P2PDB shutdown..");

    // Finish with
    stopMessageProcessor();
}

bool P2PManager::haveAnyPeers() {
    if (!P2PParams::DEFAULT_NODE_LIST.empty()) {
        return true;
    }
    if (mPeersChecker && mPeersChecker->haveAnyPeers()) {
        return true;
    }
    //
    // FIX 8: getP2PDB() returns a reference, use dot (.) operator
    //
    if (!org::minima::database::MinimaDB::getDB()->getP2PDB().getPeersList().empty()) {
        return true;
    }
    if (!mState->getKnownPeers().empty()) {
        return true;
    }
    return false;
}

bool P2PManager::inetAddrEqual(const InetSocketAddress& a, const InetSocketAddress& b) {
    //
    // FIX 4: hostStringFrom fix (FIX 2) resolves the error here
    //
    return a.getPort() == b.getPort() && hostStringFrom(a) == hostStringFrom(b);
}

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
