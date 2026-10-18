#include "org/minima/system/network/p2p2/p2_p2_d_b.hpp"

#include <algorithm>
#include <any>

#include "org/minima/utils/mini_file.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p2 {

using org::minima::utils::MiniFile;
using org::minima::utils::MinimaLogger;
using org::minima::utils::json::JSONArray;

P2P2DB::P2P2DB() : org::minima::utils::JsonDB(), mAllPeers() {}

void P2P2DB::loadDB(const std::string& filePath) {
    // Load the serialized object (Streamable) from file
    MiniFile::loadObjectSlow(std::filesystem::path(filePath), *this);

    // Reset internal peers list
    mAllPeers.clear();

    // Extract and convert the peers JSON array to internal list
    JSONArray allpeers = getJSONArray("all_known_peers");
    for (const auto& peerAny : allpeers.elements()) {
        // Expect strings; bad_any_cast will propagate similar to Java ClassCastException
        const std::string& strpeer = std::any_cast<const std::string&>(peerAny);
        mAllPeers.push_back(strpeer);
    }
}

void P2P2DB::saveDB(const std::string& filePath) {
    // Convert the internal list to a JSON array
    JSONArray peerslist;
    for (const auto& peer : mAllPeers) {
        peerslist.add(peer);
    }

    // Store in the JSON DB
    setJSONArray("all_known_peers", peerslist);

    // Save the serialized object (Streamable) to file
    MiniFile::saveObjectDirect(std::filesystem::path(filePath), *this);
}

bool P2P2DB::isFirstStartUp() const {
    return getBoolean("first_startup", true);
}

void P2P2DB::setFirstStartUp(bool zSet) {
    setBoolean("first_startup", zSet);
}

std::vector<std::string>& P2P2DB::getAllKnownPeers() {
    return mAllPeers;
}

const std::vector<std::string>& P2P2DB::getAllKnownPeers() const {
    return mAllPeers;
}

bool P2P2DB::addPeerToAllKnown(const std::string& zPeer) {
    auto it = std::find(mAllPeers.begin(), mAllPeers.end(), zPeer);
    if (it == mAllPeers.end()) {
        mAllPeers.push_back(zPeer);
        MinimaLogger::log(std::string("Peer added : ") + zPeer);
        return true;
    }
    MinimaLogger::log(std::string("Peer already added : ") + zPeer);
    return false;
}

} // namespace p2p2
} // namespace network
} // namespace system
} // namespace minima
} // namespace org