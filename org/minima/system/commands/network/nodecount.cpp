#include "org/minima/system/commands/network/nodecount.hpp"

#include <unordered_set>
#include <fstream>
#include <sstream>
#include <limits>
#include <cmath>

#include "org/minima/objects/greeting.hpp"
#include "org/minima/system/network/minima/n_i_o_manager.hpp"
#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/system/network/p2p/params/p2_p_params.hpp"
#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace network {

using org::minima::objects::Greeting;
using org::minima::system::network::minima::NIOManager;
using org::minima::system::network::p2p::messages::InetSocketAddress;
using org::minima::system::network::p2p::messages::InetSocketAddressIO;
using P2PParamsParamsAddr = org::minima::system::network::p2p::params::InetSocketAddress;

// Helpers for hashing and equality of messages::InetSocketAddress
namespace {

static std::string canonicalHost(const InetSocketAddress& addr) {
    const std::string& orig = addr.originalHost();
    if (!orig.empty()) {
        return orig;
    }
    return addr.getAddress().getHostAddress();
}

struct AddrHash {
    std::size_t operator()(const InetSocketAddress& a) const noexcept {
        std::hash<std::string> sh;
        std::hash<int> ih;
        // Combine host and port
        return (sh(canonicalHost(a)) * 1315423911u) ^ (ih(a.getPort()) + 0x9e3779b97f4a7c15ULL + (sh(canonicalHost(a)) << 6) + (sh(canonicalHost(a)) >> 2));
    }
};

struct AddrEq {
    bool operator()(const InetSocketAddress& a, const InetSocketAddress& b) const noexcept {
        return a.getPort() == b.getPort() && canonicalHost(a) == canonicalHost(b);
    }
};

using PeerSet = std::unordered_set<InetSocketAddress, AddrHash, AddrEq>;

static PeerSet setDifference(const PeerSet& a, const PeerSet& b) {
    PeerSet res;
    for (const auto& x : a) {
        if (b.find(x) == b.end()) {
            res.insert(x);
        }
    }
    return res;
}

static float calcScanPercent(std::size_t pinged, std::size_t totalPeers, bool completeMode) {
    if (totalPeers == 0) {
        return std::numeric_limits<float>::infinity();
    }
    float ratio = static_cast<float>(pinged) / static_cast<float>(totalPeers);
    if (!completeMode) {
        ratio = ratio / 0.06f;
    }
    return ratio * 100.0f;
}

static double parseDoubleFromAny(const std::any& val, double def) {
    try {
        if (val.type() == typeid(double)) {
            return std::any_cast<double>(val);
        } else if (val.type() == typeid(float)) {
            return static_cast<double>(std::any_cast<float>(val));
        } else if (val.type() == typeid(int)) {
            return static_cast<double>(std::any_cast<int>(val));
        } else if (val.type() == typeid(std::int64_t)) {
            return static_cast<double>(std::any_cast<std::int64_t>(val));
        } else if (val.type() == typeid(std::uint64_t)) {
            return static_cast<double>(std::any_cast<std::uint64_t>(val));
        } else if (val.type() == typeid(std::string)) {
            const std::string& s = std::any_cast<const std::string&>(val);
            try {
                return std::stod(s);
            } catch (...) {
                return def;
            }
        }
    } catch (...) {
        return def;
    }
    return def;
}

static std::string hostForPing(const InetSocketAddress& addr) {
    const std::string& orig = addr.originalHost();
    if (!orig.empty()) return orig;
    return addr.getAddress().getHostAddress();
}

} // anonymous namespace

nodecount::nodecount()
    : org::minima::system::commands::Command(
          "nodecount",
          "(file:) (complete:false|true) - Enumerate the network to count all participating nodes") {}

std::vector<std::string> nodecount::getValidParams() const {
    return {"file", "complete"};
}

std::unique_ptr<org::minima::utils::json::JSONObject> nodecount::runCommand() {
    using org::minima::utils::json::JSONArray;
    using org::minima::utils::json::JSONObject;

    bool saveCSV = true;
    std::string file;
    try {
        file = getParam("file", "");
    } catch (...) {
        // If param subsystem throws, default to empty
        file = "";
    }

    std::unique_ptr<std::ofstream> writer;
    JSONObject details;

    if (file.empty()) {
        saveCSV = false;
    } else {
        std::filesystem::path csvFile = org::minima::utils::MiniFile::createBaseFile(file);
        writer = std::make_unique<std::ofstream>(csvFile, std::ios::out | std::ios::trunc);
        if (!(*writer)) {
            // If file cannot be opened, disable CSV saving but continue
            saveCSV = false;
        }
    }

    bool complete = false;
    try {
        complete = getBooleanParam("complete", false);
    } catch (...) {
        complete = false;
    }

    auto ret = getJSONReply();

    PeerSet allPeers;
    PeerSet pingedPeers;

    // Initial sweep over default nodes
    for (const P2PParamsParamsAddr& addr : org::minima::system::network::p2p::params::P2PParams::DEFAULT_NODE_LIST) {
        std::shared_ptr<Greeting> greet = NIOManager::sendPingMessage(addr.host, addr.port, true);
        if (greet) {
            JSONObject& extra = greet->getExtraData();
            if (extra.containsKey("peers-list")) {
                const std::any& aval = extra.get("peers-list");
                if (const JSONArray* parr = std::any_cast<JSONArray>(&aval)) {
                    std::vector<InetSocketAddress> newPeers = InetSocketAddressIO::addressesJSONArrayToList(*parr);
                    for (const auto& np : newPeers) {
                        allPeers.insert(np);
                    }
                }
            }
        }
        // Record this default node as pinged
        allPeers.insert(InetSocketAddress(addr.host, addr.port)); // Optional: sometimes peers list may not include it
        pingedPeers.insert(InetSocketAddress(addr.host, addr.port));
    }

    bool keepGoing = true;
    PeerSet peersToCheck = setDifference(allPeers, pingedPeers);
    double totalClients = 0.0;

    if (saveCSV && writer && (*writer)) {
        (*writer) << "num_checked,total_peers,num_clients\n";
        writer->flush();
    }

    float scan_percent = calcScanPercent(pingedPeers.size(), allPeers.size(), false);

    while (keepGoing) {
        for (const auto& addr : peersToCheck) {
            std::shared_ptr<Greeting> greet = NIOManager::sendPingMessage(hostForPing(addr), addr.getPort(), true);
            if (greet) {
                JSONObject& extra = greet->getExtraData();

                // Clients
                double addClients = 0.0;
                if (extra.containsKey("clients")) {
                    addClients = parseDoubleFromAny(extra.get("clients"), 0.0);
                }
                totalClients += addClients;

                // Peers list
                if (extra.containsKey("peers-list")) {
                    const std::any& aval = extra.get("peers-list");
                    if (const JSONArray* parr = std::any_cast<JSONArray>(&aval)) {
                        std::vector<InetSocketAddress> newPeers = InetSocketAddressIO::addressesJSONArrayToList(*parr);
                        for (const auto& np : newPeers) {
                            allPeers.insert(np);
                        }
                    }
                }
            }

            pingedPeers.insert(addr);

            scan_percent = calcScanPercent(pingedPeers.size(), allPeers.size(), false);

            if (saveCSV && writer && (*writer)) {
                (*writer) << pingedPeers.size() << "," << allPeers.size() << "," << totalClients << "\n";
                writer->flush();
            }

            if (scan_percent >= 100.0f && !complete) {
                keepGoing = false;
                break;
            }

            if (complete) {
                // In complete mode show raw percentage of discovered peers checked.
                scan_percent = calcScanPercent(pingedPeers.size(), allPeers.size(), true);
            }

            org::minima::utils::MinimaLogger::log(
                std::string("Peers Checked: ") + std::to_string(pingedPeers.size()) +
                " Scanning... " + std::to_string(static_cast<int>(scan_percent)) + "%");
        }

        peersToCheck = setDifference(allPeers, pingedPeers);

        if (peersToCheck.empty() || scan_percent >= 100.0f) {
            keepGoing = false;

            float clients_per_server = 0.0f;
            if (!pingedPeers.empty()) {
                clients_per_server = static_cast<float>(totalClients) / static_cast<float>(pingedPeers.size());
            }

            // After 6% of the network has been scanned over 95% of p2p nodes have been discovered
            // multiplying the total peers to give a projected total node count.
            float total_servers = static_cast<float>(allPeers.size()) * 1.05f;
            float clients = clients_per_server * total_servers;
            float total_nodes = total_servers + clients;

            if (complete) {
                total_servers = static_cast<float>(allPeers.size());
                clients = static_cast<float>(totalClients);
                total_nodes = total_servers + clients;
            }

            org::minima::utils::MinimaLogger::log(
                std::string("Total participating nodes: ") + std::to_string(static_cast<int>(total_nodes)) +
                " of which " + std::to_string(static_cast<int>(total_servers)) +
                " are p2p servers and " + std::to_string(static_cast<int>(clients)) + " are client nodes.");

            details.put("total_nodes", static_cast<double>(total_nodes));
            details.put("server_nodes", static_cast<double>(total_servers));
            details.put("client_nodes", static_cast<double>(clients));
        }
    }

    ret->put("response", details);
    return ret;
}

org::minima::system::commands::Command* nodecount::getFunction() {
    return new nodecount();
}

} // namespace network
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org