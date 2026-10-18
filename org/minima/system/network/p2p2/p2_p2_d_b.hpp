#pragma once

#include <string>
#include <vector>

#include "org/minima/utils/json_d_b.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p2 {

class P2P2DB : public org::minima::utils::JsonDB {
public:
    P2P2DB();

    // Load/Save database from/to a filesystem path (string form)
    // Performs conversion of "all_known_peers" JSON array into mAllPeers on load
    // and writes mAllPeers back into JSON on save.
    void loadDB(const std::string& filePath);
    void saveDB(const std::string& filePath);

    // First startup flag
    bool isFirstStartUp() const;
    void setFirstStartUp(bool zSet);

    // Access the internal peers list (mutable, mirrors Java behavior)
    std::vector<std::string>& getAllKnownPeers();
    const std::vector<std::string>& getAllKnownPeers() const;

    // Add a peer if not already present; logs and returns true when added,
    // logs and returns false if already present.
    bool addPeerToAllKnown(const std::string& zPeer);

private:
    std::vector<std::string> mAllPeers;
};

} // namespace p2p2
} // namespace network
} // namespace system
} // namespace minima
} // namespace org