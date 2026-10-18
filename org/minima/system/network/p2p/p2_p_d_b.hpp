#ifndef ORG_MINIMA_SYSTEM_NETWORK_P2P_P2_P_D_B_HPP
#define ORG_MINIMA_SYSTEM_NETWORK_P2P_P2_P_D_B_HPP
#pragma once

#include <string>
#include <vector>

#include "org/minima/utils/json_d_b.hpp"

// Forward declarations for project dependencies (per skeleton rules)
namespace org { namespace minima { namespace system { namespace network { namespace p2p { namespace messages {
struct InetSocketAddress;
} } } } } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

class P2PDB : public org::minima::utils::JsonDB {
public:
    P2PDB();

    // Loads the peers list from the DB
    // Returns a list of peers or an empty list if there is no data to load
    std::vector<org::minima::system::network::p2p::messages::InetSocketAddress> getPeersList();

    // Stores a list of InetSocketAddress peers as json array of strings in the db
    // peers: vector of discovery peers host:minimaPort
    void setPeersList(const std::vector<org::minima::system::network::p2p::messages::InetSocketAddress>& peers);

    // Sets the version number for the database using the P2PParams value
    void setVersion();

    // Gets the version number of data saved in the save file.
    // If there is no data then default to current version.
    std::string getVersion() const;
};

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org

#endif // ORG_MINIMA_SYSTEM_NETWORK_P2P_P2_P_D_B_HPP