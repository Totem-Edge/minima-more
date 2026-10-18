#pragma once

#include <string>
#include <vector>

// Include needed for P2PFunctions::Level type
#include "org/minima/system/network/p2p/p2_p_functions.hpp"

namespace org { namespace minima { namespace system { namespace network { namespace p2p { namespace params {

struct InetSocketAddress {
    std::string host;
    int port;

    InetSocketAddress() : host(), port(0) {}
    InetSocketAddress(std::string h, int p) : host(std::move(h)), port(p) {}
};

class P2PParams {
public:
    // P2P Version number (Major - breaking changes. Minor - none breaking changes)
    static std::string VERSION;

    // P2P Log level
    static org::minima::system::network::p2p::P2PFunctions::Level LOG_LEVEL;

    // Max number of peers to keep in the peers list
    static int PEERS_LIST_SIZE;

    // Desired number of in link and out links to maintain
    static int TGT_NUM_LINKS;

    // Desired number of client (nodes that can't accept inbound connections) to maintain
    static int TGT_NUM_NONE_P2P_LINKS;

    // Desired number of connections clients should maintain
    static int MIN_NUM_CONNECTIONS;

    // Time between P2P system assessing its state in milliseconds
    static int LOOP_DELAY;

    // Time between updating the device hash_rate in milliseconds
    static int HASH_RATE_UPDATE_DELAY;

    // Max additional ms to add to loop delay
    static int LOOP_DELAY_VARIABILITY;

    // Time between P2P system assessing if it can receive inbound connections milliseconds
    static int NODE_NOT_ACCEPTING_CHECK_DELAY;

    static int SAVE_DATA_DELAY;

    // Time in ms before walk link messages expire
    static int WALK_LINKS_EXPIRE_TIME;

    // Time before auth key to expire - required to accept DoSwap and walk based connections
    static int AUTH_KEY_EXPIRY;

    static int METRICS_DELAY;

    // No default peers by default
    static std::vector<InetSocketAddress> DEFAULT_NODE_LIST;
};

} } } } } }