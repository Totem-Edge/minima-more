#include "org/minima/system/network/p2p/p2_p_d_b.hpp"

#include <utility>

#include "org/minima/system/network/p2p/messages/inet_socket_address_i_o.hpp"
#include "org/minima/system/network/p2p/params/p2_p_params.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace p2p {

using org::minima::system::network::p2p::messages::InetSocketAddress;
using org::minima::system::network::p2p::messages::InetSocketAddressIO;
using org::minima::utils::json::JSONArray;

P2PDB::P2PDB() : org::minima::utils::JsonDB() {}

std::vector<InetSocketAddress> P2PDB::getPeersList() {
    std::vector<InetSocketAddress> peers;

    // Load the array (empty if absent)
    JSONArray jsonArray = getJSONArray("peers");

    if (jsonArray.size() != 0) {
        // Convert JSON array into list of InetSocketAddress
        auto list = InetSocketAddressIO::addressesJSONToList(jsonArray);
        // Preserve original behavior of adding all to a new list
        peers.insert(peers.end(), list.begin(), list.end());
    }

    return peers;
}

void P2PDB::setPeersList(const std::vector<InetSocketAddress>& peers) {
    // Convert list to JSON array and store
    JSONArray jarr = InetSocketAddressIO::addressesListToJSON(peers);
    setJSONArray("peers", jarr);
}

void P2PDB::setVersion() {
    setString("version", org::minima::system::network::p2p::params::P2PParams::VERSION);
}

std::string P2PDB::getVersion() const {
    return getString("version", org::minima::system::network::p2p::params::P2PParams::VERSION);
}

} // namespace p2p
} // namespace network
} // namespace system
} // namespace minima
} // namespace org