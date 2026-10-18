#include "org/minima/system/network/p2p/p2_p_peers_checker.hpp"

#include <algorithm>
#include <iterator>
#include <memory> // <-- FIX: Add for std::make_shared

// Messages
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/messages/timer_message.hpp"

// Logging and params
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/system/params/global_params.hpp"

// DB and chain
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_po_w_tree_node.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

// Objects needed
#include "org/minima/objects/greeting.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// FIX: Add full headers for incomplete types
#include "org/minima/objects/base/mini_string.hpp"
#include "org/minima/system/network/minima/n_i_o_client_info.hpp"

// P2P manager and functions
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/system/network/p2p/p2_p_functions.hpp"

// NIO
#include "org/minima/system/network/minima/n_i_o_manager.hpp"

using org::minima::utils::messages::Message;
using org::minima::utils::messages::TimerMessage;
using org::minima::utils::MinimaLogger;
using org::minima::system::params::GeneralParams;
using org::minima::system::params::GlobalParams;
using org::minima::database::MinimaDB;
using org::minima::database::txpowtree::TxPoWTreeNode;
using org::minima::objects::Greeting;
using org::minima::utils::json::JSONObject;
using org::minima::objects::base::MiniNumber;

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

int P2PPeersChecker::MAX_VERIFIED_PEERS = 250;

P2PPeersChecker::P2PPeersChecker(org::minima::system::network::p2p::P2PManager* manager)
: org::minima::utils::messages::MessageProcessor("PEERS_CHECKER")
, m_p2pManager(manager)
, m_rng(std::random_device{}()) {

    setFullLogging(false, "");

    // Do some initialization
    // FIX: PostMessage expects a std::shared_ptr
    auto initmsg = std::make_shared<Message>(PEERS_INIT);
    PostMessage(initmsg);

    // First one happens after 2 hours (commented in Java)
    // auto tmsg = std::make_shared<TimerMessage>(static_cast<std::int64_t>(1000) * 60 * 60 * 2, PEERS_LOOP);
    // PostTimerMessage(tmsg);

    startMessageProcessorThread();
}

const std::unordered_set<InetSocketAddress>& P2PPeersChecker::getUnverifiedPeers() const {
    return m_unverifiedPeers;
}

const std::unordered_set<InetSocketAddress>& P2PPeersChecker::getVerifiedPeers() const {
    return m_verifiedPeers;
}

std::unordered_set<InetSocketAddress>& P2PPeersChecker::getVerifiedPeers() {
    return m_verifiedPeers;
}

bool P2PPeersChecker::haveAnyPeers() const {
    return (!m_verifiedPeers.empty() || !m_unverifiedPeers.empty());
}

void P2PPeersChecker::checkUnverifiedPeer(const InetSocketAddress& zAddress) {
    // Do we know it
    if (m_unverifiedPeers.find(zAddress) != m_unverifiedPeers.end() ||
        m_verifiedPeers.find(zAddress)   != m_verifiedPeers.end()) {
        return;
    }

    // Check the limit
    if (static_cast<int>(m_unverifiedPeers.size()) < MAX_VERIFIED_PEERS) {
        // Do we have all the verified peers?
        if (static_cast<int>(m_verifiedPeers.size()) >= MAX_VERIFIED_PEERS) {
            // Randomly choose if to add it.. 10% chance
            std::uniform_int_distribution<int> dist(0, 99);
            int randv = dist(m_rng);
            if (randv < 90) {
                // MOST will not be added
                return;
            }
        }

        // Add it to the list
        m_unverifiedPeers.insert(zAddress);

        // Send a message to check it
        // FIX: PostMessage expects a std::shared_ptr
        auto msg = std::make_shared<Message>(PEERS_CHECKPEERS);
        msg->addObject("address", zAddress);
        PostMessage(msg);
    } else {
        // MAX reached.. (no action as per Java)
    }
}

bool P2PPeersChecker::removeRandomItem(std::unordered_set<InetSocketAddress>& zSet, InetSocketAddress& outRemoved) {
    int size = static_cast<int>(zSet.size());
    if (size == 0) {
        return false;
    }

    std::uniform_int_distribution<int> dist(0, size - 1);
    int item = dist(m_rng);

    int i = 0;
    for (auto it = zSet.begin(); it != zSet.end(); ++it, ++i) {
        if (i == item) {
            outRemoved = *it;
            zSet.erase(it);
            return true;
        }
    }
    return false;
}

void P2PPeersChecker::processMessage(Message& zMessage) {
    const std::string& type = zMessage.getMessageType();

    if (type == PEERS_INIT) {
        // No-op as in Java

    } else if (type == PEERS_ADDPEERS) {
        // When a new peer address is added - check if already in verified or unverified.
        // If not, add to unverified and request a contact check.
        auto anyaddr = zMessage.getObject("address");
        InetSocketAddress address;
        try {
            address = std::any_cast<InetSocketAddress>(anyaddr);
        } catch (const std::bad_any_cast&) {
            // Type mismatch: cannot process
            return;
        }

        bool islocal = P2PFunctions::isIPLocal(address.getAddress().getHostAddress());
        bool isipv6  = P2PFunctions::isIPv6(address.getAddress().getHostAddress());
        if (GeneralParams::ALLOW_ALL_IP || (!islocal && !isipv6)) {
            checkUnverifiedPeer(address);
        } else {
            P2PFunctions::log_debug(std::string("[-] Prevent node from adding localhost / ipv6 address to peers list ") + address.getAddress().getHostAddress());
        }

    } else if (type == PEERS_CHECKPEERS) {
        auto anyaddr = zMessage.getObject("address");
        InetSocketAddress address;
        try {
            address = std::any_cast<InetSocketAddress>(anyaddr);
        } catch (const std::bad_any_cast&) {
            return;
        }

        bool force = false;
        if (zMessage.exists("force")) {
            force = zMessage.getBoolean("force");
        }

        bool forcelog = false;
        if (zMessage.exists("forcelog")) {
            forcelog = zMessage.getBoolean("forcelog");
        }
        // Connectivity gate: either forced or there is at least one connected connection
        if (force || (P2PFunctions::getAllConnectedConnections().size() > 0)) {
            // Get a Greeting if possible (best effort, for version/chain info)
            std::shared_ptr<Greeting> greet = org::minima::system::network::minima::NIOManager::sendPingMessage(
                address.getAddress().getHostAddress(), address.getPort(), true);

            bool validversion = false;

            if (greet) {
                bool testcheck = true;
                std::string greetstr = greet->getVersion().toString();
                if (GeneralParams::TEST_PARAMS && greetstr.find("TEST") == std::string::npos) {
                    testcheck = false;
                } else if (!GeneralParams::TEST_PARAMS && greetstr.find("TEST") != std::string::npos) {
                    testcheck = false;
                }

                if (testcheck && greetstr.find(GlobalParams::MINIMA_BASE_VERSION) == 0) {
                    validversion = true;
                }

                // Chain check (relaxed for bootstrap / force case)
                if (validversion) {
                    JSONObject& extra = greet->getExtraData();
                    std::string block     = extra.getString("50block", "");
                    std::string blockhash = extra.getString("50hash", "");

                    if (!block.empty() && !blockhash.empty()) {
                        auto& tree = MinimaDB::getDB()->getTxPoWTree();
                        auto tip = tree.getTip();

                        if (tip) {
                            std::shared_ptr<MiniNumber> mytipblock = std::make_shared<MiniNumber>(tip->getBlockNumber());
                            if (!mytipblock->isLess(MiniNumber(block))) {
                                auto checknode = tip->getPastNode(MiniNumber(block));
                                if (checknode) {
                                    if (checknode->getTxPoW().getTxPoWID() != blockhash) {
                                        if (forcelog || GeneralParams::PEERSCHECKER_lOG) {
                                            MinimaLogger::log(std::string("PEERS CHECKER incorrect chain! @ ") + block + " " + address.toString());
                                        }
                                        validversion = false;
                                    }
                                } else {
                                    validversion = false;
                                }
                            }
                            // if we are behind, still accept
                        }
                    }
                }
            } else {
                if (forcelog) {
                    MinimaLogger::log(std::string("NULL greeting from Peer ") + address.toString() + " (will still trust on force)");
                }
            }

            // For forced adds (user explicitly listed the peer), still require a
            // successful ping and compatible version. Force only bypasses the
            // connectivity-gate that requires at least one live connection.
            bool shouldAdd = validversion;

            if (shouldAdd) {
                m_unverifiedPeers.erase(address);

                if (static_cast<int>(m_verifiedPeers.size()) >= MAX_VERIFIED_PEERS) {
                    InetSocketAddress removed;
                    if (removeRandomItem(m_verifiedPeers, removed)) {
                        if (m_p2pManager) {
                            auto msg = std::make_shared<Message>(org::minima::system::network::p2p::P2PManager::P2P_REMOVE_PEER);
                            msg->addObject("address", removed);
                            m_p2pManager->PostMessage(msg);
                        }
                    }
                }

                m_verifiedPeers.erase(address);
                m_verifiedPeers.insert(address);

                if (m_p2pManager) {
                    auto msg = std::make_shared<Message>(org::minima::system::network::p2p::P2PManager::P2P_ADD_PEER);
                    msg->addObject("address", address);
                    m_p2pManager->PostMessage(msg);
                }
            } else {
                if (m_verifiedPeers.find(address) != m_verifiedPeers.end()) {
                    m_verifiedPeers.erase(address);
                    if (m_verifiedPeers.empty()) {
                        P2PFunctions::log_node_runner("[-] All addresses removed from verified peers list - Check node has internet connection");
                    }
                    m_unverifiedPeers.insert(address);

                    auto tmsg = std::make_shared<TimerMessage>(static_cast<std::int64_t>(1000) * 60 * 30, PEERS_CHECKPEERS);
                    tmsg->addObject("address", address);
                    PostTimerMessage(tmsg);
                } else {
                    m_unverifiedPeers.erase(address);
                }

                if (m_p2pManager) {
                    auto msg = std::make_shared<Message>(org::minima::system::network::p2p::P2PManager::P2P_REMOVE_PEER);
                    msg->addObject("address", address);
                    m_p2pManager->PostMessage(msg);
                }
            }

        } else {
            // Not forced and no current connections: retry later
            auto tmsg = std::make_shared<TimerMessage>(static_cast<std::int64_t>(60'000), PEERS_CHECKPEERS);
            tmsg->addObject("address", address);
            PostTimerMessage(tmsg);
        }

    } else if (type == PEERS_LOOP) {
        // Check we have a net connection
        if (P2PFunctions::isNetAvailable()) {
            // Check all the verified Peers again
            for (const InetSocketAddress& address : m_verifiedPeers) {
                // FIX: PostMessage expects a std::shared_ptr
                auto msg = std::make_shared<Message>(PEERS_CHECKPEERS);
                msg->addObject("address", address);
                PostMessage(msg);
            }
        }

        // Do it again ..
        auto tmsg = std::make_shared<TimerMessage>(m_PEERS_LOOP_TIMER, PEERS_LOOP);
        PostTimerMessage(tmsg);

    } else if (type == PEERS_FORCEFULLCHECK) {
        // Check we have a net connection
        if (P2PFunctions::isNetAvailable()) {
            // Check all the verified Peers again
            for (const InetSocketAddress& address : m_verifiedPeers) {
                // FIX: PostMessage expects a std::shared_ptr
                auto msg = std::make_shared<Message>(PEERS_CHECKPEERS);
                msg->addObject("address", address).addBoolean("forcelog", true);
                PostMessage(msg);
            }
        } else {
            MinimaLogger::log("No Network connection - cannot perform peers check");
        }
    }
}

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org
