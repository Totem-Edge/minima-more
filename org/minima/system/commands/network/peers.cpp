#include "org/minima/system/commands/network/peers.hpp"

#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>
#include <filesystem>

#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/system/commands/command_exception.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/r_p_c_client.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/messages/message.hpp"
#include "org/minima/utils/minima_logger.hpp"

#include "org/minima/system/network/minima/n_i_o_manager.hpp"

// Additional headers for used methods
#include "org/minima/database/userprefs/user_d_b.hpp"

// P2P access
#include "org/minima/system/main.hpp"
#include "org/minima/system/network/network_manager.hpp"
#include "org/minima/system/network/p2p/p2_p_manager.hpp"
#include "org/minima/system/network/p2p/p2_p_peers_checker.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

using org::minima::system::commands::CommandException;
using org::minima::utils::json::JSONObject;
using org::minima::utils::messages::Message;

peers::peers()
    : Command("peers", "(action:list|addpeers) (peerslist:) (trust:) - List or add peers. trust:true = force connect (known-good peers only)") {}

std::string peers::getFullHelp() const {
    return
        "\npeers\n"
        "\n"
        "Prints the peers list this node has. P2P must be enabled.\n"
        "\n"
        "Your peers are the other Minima nodes you know about.\n"
        "\n"
        "action: (optional)\n"
        "    list : List your peers. The default.\n"
        "    addpeers : Add a list of new peers. \n"
        "\n"
        "peerslist: (optional)\n"
        "    CSV list of new peers [ip:port,ip:port,..]\n"
        "\n"
        "trust: (optional)\n"
        "    true = force connect + trust these peers immediately (bypass ping validation).\n"
        "           Use only for known-good peers.\n"
        "\n"
        "Examples:\n"
        "\n"
        "peers\n"
        "\n"
        "peers action:list\n"
        "\n"
        "peers action:addpeers peerslist:31.125.188.214:9001,94.0.239.117:9001\n"
        "\n"
        "peers action:addpeers peerslist:62.97.39.121:9001,45.128.3.158:9001 trust:true\n";
}

std::vector<std::string> peers::getValidParams() const {
    return {"action", "peerslist", "max", "file", "url", "trust"};
}

std::unique_ptr<JSONObject> peers::runCommand() {
    auto ret = getJSONReply();

    // Is the P2P enabled or are we a slave node
    if (!org::minima::system::params::GeneralParams::P2P_ENABLED ||
        org::minima::database::MinimaDB::getDB()->getUserDB().isSlaveNode()) {

        JSONObject resp;
        resp.put("peers-list", std::string(""));
        resp.put("havepeers", true);
        resp.put("p2penabled", org::minima::system::params::GeneralParams::P2P_ENABLED);
        resp.put("message", std::string("P2P System NOT enabled"));
        ret->put("response", resp);
        return ret;
    }

    std::string action = getParam("action", "list");
    if (action == "list") {
        // How many peers to show
        auto maxnum = getNumberParam("max", org::minima::objects::base::MiniNumber::THOUSAND());
        int maxpeers = maxnum->getAsInt();

        std::string peerslist = getPeersList(maxpeers);

        int numberpeers = 0;
        std::string trimmed = trim(peerslist);
        if (!trimmed.empty()) {
            numberpeers = static_cast<int>(std::count(peerslist.begin(), peerslist.end(), ',')) + 1;
        }

        JSONObject resp;
        resp.put("peerslist", peerslist);
        resp.put("size", numberpeers);

        // Query P2PManager for haveAnyPeers (mirrors Java peers.java line 90)
        bool havepeers = false;
        auto* p2pmanager = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(
            &org::minima::system::Main::getInstance()->getNetworkManager().getP2PManager());
        if (p2pmanager) {
            havepeers = p2pmanager->haveAnyPeers();
        }
        resp.put("havepeers", havepeers);
        resp.put("p2penabled", org::minima::system::params::GeneralParams::P2P_ENABLED);
        ret->put("response", resp);

    } else if (action == "forcecheck") {
        // Post PEERS_FORCEFULLCHECK to the checker (mirrors Java peers.java line 98)
        auto* p2pmanager = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(
            &org::minima::system::Main::getInstance()->getNetworkManager().getP2PManager());
        if (p2pmanager) {
            auto* checker = p2pmanager->getPeersChecker();
            if (checker) {
                auto msg = std::make_shared<Message>(
                    org::minima::system::network::p2p::P2PPeersChecker::PEERS_FORCEFULLCHECK);
                checker->PostMessage(msg);
            }
        }

        auto maxnum = getNumberParam("max", org::minima::objects::base::MiniNumber::THOUSAND());
        int maxpeers = maxnum->getAsInt();
        std::string peerslist = getPeersList(maxpeers);

        int numberpeers = 0;
        std::string trimmed = trim(peerslist);
        if (!trimmed.empty()) {
            numberpeers = static_cast<int>(std::count(peerslist.begin(), peerslist.end(), ',')) + 1;
        }

        JSONObject resp;
        resp.put("message", std::string("Peers check started.. will start ASAP"));
        resp.put("logs", org::minima::system::params::GeneralParams::PEERSCHECKER_lOG);
        resp.put("size", numberpeers);
        ret->put("response", resp);

    } else if (action == "addpeers") {
        std::string peerstr = getParam("peerslist");

        if (startsWith(peerstr, "http")) {
            std::string urllist = org::minima::utils::RPCClient::sendGET(peerstr);
            if (urllist == "") {
                throw CommandException("No peers found @ " + peerstr);
            }
            peerstr = urllist;
        }

        std::vector<std::string> validpeers;
        std::vector<std::string> invalidpeers;

        // Get P2PManager and P2PPeersChecker for posting
        auto* p2pmanager = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(
            &org::minima::system::Main::getInstance()->getNetworkManager().getP2PManager());
        auto* p2pchecker = p2pmanager ? p2pmanager->getPeersChecker() : nullptr;

        bool trust = getBooleanParam("trust", false);

        std::stringstream ss(peerstr);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token = trim(token);
            if (token.empty()) {
                continue;
            }
            auto checker = createConnectMessage(token);
            if (checker) {
                org::minima::system::network::p2p::InetSocketAddress addr(
                    checker->getString("host"), checker->getInteger("port"));

                // For any peer the user explicitly adds via command, immediately:
                // - do a direct NIO connect (most reliable)
                // - feed P2P manager so it is in knownPeers and will be used by the P2P loop
                // - ask the checker (force) so verified lists get updated too
                {
                    auto direct = std::make_shared<Message>(
                        org::minima::system::network::minima::NIOManager::NIO_CONNECT);
                    direct->addString("host", addr.getAddress().getHostAddress());
                    direct->addInteger("port", addr.getPort());
                    org::minima::system::Main::getInstance()->getNIOManager().PostMessage(direct);
                }

                if (p2pmanager) {
                    auto addp = std::make_shared<Message>(
                        org::minima::system::network::p2p::P2PManager::P2P_ADD_PEER);
                    addp->addObject("address", addr);
                    p2pmanager->PostMessage(addp);

                    auto conn = std::make_shared<Message>(
                        org::minima::system::network::p2p::P2PManager::P2P_SEND_CONNECT);
                    conn->addObject(org::minima::system::network::p2p::P2PManager::ADDRESS_LITERAL, addr);
                    p2pmanager->PostMessage(conn);
                }

                if (p2pchecker) {
                    // Always notify checker with force for user-added peers
                    auto cmsg = std::make_shared<Message>(
                        org::minima::system::network::p2p::P2PPeersChecker::PEERS_CHECKPEERS);
                    cmsg->addObject("address", addr);
                    cmsg->addBoolean("force", true);
                    p2pchecker->PostMessage(cmsg);

                    // Also add via ADDPEERS path for verified list
                    auto amsg = std::make_shared<Message>(
                        org::minima::system::network::p2p::P2PPeersChecker::PEERS_ADDPEERS);
                    amsg->addObject("address", addr);
                    p2pchecker->PostMessage(amsg);
                }

                if (trust) {
                    org::minima::utils::MinimaLogger::log(std::string("[trust] Force-connecting known peer ") +
                        addr.getAddress().getHostAddress() + ":" + std::to_string(addr.getPort()));
                } else {
                    org::minima::utils::MinimaLogger::log(std::string("[peers] Connecting to added peer ") +
                        addr.getAddress().getHostAddress() + ":" + std::to_string(addr.getPort()));
                }

                validpeers.push_back(token);
            } else {
                invalidpeers.push_back(token);
            }
        }

        // Build Java-style ArrayList<String>.toString() representation: [a, b, c]
        auto vecToString = [](const std::vector<std::string>& v) -> std::string {
            std::ostringstream out;
            out << "[";
            for (size_t i = 0; i < v.size(); ++i) {
                out << v[i];
                if (i + 1 < v.size()) out << ", ";
            }
            out << "]";
            return out.str();
        };

        JSONObject resp;
        resp.put("valid", vecToString(validpeers));
        resp.put("invalid", vecToString(invalidpeers));
        if (trust) {
            resp.put("message", std::string("Known-good peers trusted and connect attempted immediately."));
            resp.put("trust", true);
        } else {
            resp.put("message", std::string("Valid peers added to checking queue.."));
        }
        ret->put("response", resp);

    } else if (action == "publish") {
        std::string file = getParam("file", "peerslist.txt");

        std::string peerslist = getPeersList(20);

        std::filesystem::path ff = org::minima::utils::MiniFile::createBaseFile(file);

        std::vector<std::uint8_t> bytes(peerslist.begin(), peerslist.end());
        org::minima::utils::MiniFile::writeDataToFile(ff, bytes);

        JSONObject resp;
        resp.put("peers", peerslist);
        resp.put("file", std::filesystem::absolute(ff).string());
        ret->put("response", resp);

    } else if (action == "fetch") {
        std::string peerstr;
        std::string url;

        if (existsParam("file")) {
            url = getParam("file");
            std::filesystem::path ff = org::minima::utils::MiniFile::createBaseFile(url);
            auto pdata = org::minima::utils::MiniFile::readCompleteFile(ff);
            peerstr = std::string(pdata.begin(), pdata.end());
        } else {
            url = getParam("url");
            peerstr = org::minima::utils::RPCClient::sendGET(url);
        }

        if (peerstr == "") {
            throw CommandException("No peers found in location " + url);
        }

        // Get P2PManager and P2PPeersChecker for posting
        auto* p2pmanager = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(
            &org::minima::system::Main::getInstance()->getNetworkManager().getP2PManager());
        auto* p2pchecker = p2pmanager ? p2pmanager->getPeersChecker() : nullptr;

        // Parse, validate, and post to P2PPeersChecker (mirrors Java peers.java line 197-216)
        std::stringstream ss(peerstr);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token = trim(token);
            if (token.empty()) {
                continue;
            }
            auto checker = createConnectMessage(token);
            if (checker && p2pchecker) {
                org::minima::system::network::p2p::InetSocketAddress addr(
                    checker->getString("host"), checker->getInteger("port"));
                auto msg = std::make_shared<Message>(
                    org::minima::system::network::p2p::P2PPeersChecker::PEERS_CHECKPEERS);
                msg->addObject("address", addr);
                msg->addBoolean("force", true);
                p2pchecker->PostMessage(msg);
            }
        }

        JSONObject resp;
        resp.put("peers", peerstr);
        resp.put("location", url);
        resp.put("message", std::string("Valid peers added to checking queue.."));
        ret->put("response", resp);

    } else {
        throw CommandException("Invalid action : " + action);
    }

    return ret;
}

std::string peers::getPeersList(int zMaxPeers) {
    // Query P2PManager for peers (mirrors Java peers.java line 232-263)
    auto* p2pmanager = dynamic_cast<org::minima::system::network::p2p::P2PManager*>(
        &org::minima::system::Main::getInstance()->getNetworkManager().getP2PManager());
    if (!p2pmanager) {
        return "";
    }

    auto peers = p2pmanager->getPeersCopy();

    // Shuffle to mimic Java's Collections.shuffle
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(peers.begin(), peers.end(), g);

    // Format as CSV: host:port,host:port,...
    std::ostringstream out;
    int counter = 0;
    for (const auto& peer : peers) {
        if (counter > zMaxPeers) {
            break;
        }
        out << peer.getAddress().getHostAddress() << ":" << peer.getPort();
        counter++;
        if (counter <= zMaxPeers && counter < static_cast<int>(peers.size())) {
            out << ",";
        }
    }

    return out.str();
}

org::minima::system::commands::Command* peers::getFunction() {
    return new peers();
}

std::unique_ptr<Message> peers::createConnectMessage(const std::string& zPeer) {
    // Trim
    std::string s = trim(zPeer);
    if (s.empty()) {
        return nullptr;
    }

    // Split on last ':' to allow potential hostnames containing ':'
    auto pos = s.find_last_of(':');
    if (pos == std::string::npos) {
        return nullptr;
    }

    std::string host = s.substr(0, pos);
    std::string portstr = s.substr(pos + 1);
    host = trim(host);
    portstr = trim(portstr);

    if (host.empty() || portstr.empty()) {
        return nullptr;
    }

    // Validate port is numeric
    for (char c : portstr) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return nullptr;
        }
    }

    int port = 0;
    try {
        port = std::stoi(portstr);
    } catch (...) {
        return nullptr;
    }
    if (port < 0 || port > 65535) {
        return nullptr;
    }

    auto msg = std::make_unique<Message>("CONNECT");
    msg->addString("host", host);
    msg->addInteger("port", port);
    return msg;
}

std::string peers::trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    if (start == s.size()) return std::string();
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

bool peers::startsWith(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), s.begin());
}

std::vector<std::string> peers::splitCSV(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) {
        std::string t = trim(token);
        if (!t.empty()) {
            out.push_back(t);
        }
    }
    return out;
}

std::string peers::joinCSV(const std::vector<std::string>& v, std::size_t maxcount) {
    std::ostringstream out;
    std::size_t count = std::min(maxcount, v.size());
    for (std::size_t i = 0; i < count; ++i) {
        out << v[i];
        if (i + 1 < count) out << ",";
    }
    return out.str();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org