#include "org/minima/system/network/p2p/params/p2_p_test_params.hpp"

// Include the full header for P2PParams to assign into its static members
#include "org/minima/system/network/p2p/params/p2_p_params.hpp"

namespace org { namespace minima { namespace system { namespace network { namespace p2p { namespace params {

// Static member definitions and initializations

org::minima::system::network::p2p::P2PFunctions::Level P2PTestParams::LOG_LEVEL =
    org::minima::system::network::p2p::P2PFunctions::Level::DEBUG;

int P2PTestParams::TGT_NUM_LINKS = 5;
int P2PTestParams::TGT_NUM_NONE_P2P_LINKS = 2;
int P2PTestParams::MIN_NUM_CONNECTIONS = 2;

int P2PTestParams::LOOP_DELAY = 5000;
int P2PTestParams::HASH_RATE_UPDATE_DELAY = 60 * 1000;
int P2PTestParams::LOOP_DELAY_VARIABILITY = 3000;
int P2PTestParams::NODE_NOT_ACCEPTING_CHECK_DELAY = 60 * 1000;

int P2PTestParams::SAVE_DATA_DELAY = 60 * 1000;

int P2PTestParams::WALK_LINKS_EXPIRE_TIME = 5000;
int P2PTestParams::AUTH_KEY_EXPIRY = 300000;

int P2PTestParams::METRICS_DELAY = 30000;

std::string P2PTestParams::METRICS_URL = "http://metrics:5000/network";

// Match type of P2PParams::DEFAULT_NODE_LIST (vector<InetSocketAddress>)
std::vector<InetSocketAddress> P2PTestParams::DEFAULT_NODE_LIST = {
    InetSocketAddress("minima_one", 9001)
};

void P2PTestParams::setTestParams() {
    // Assign test values into P2PParams
    P2PParams::DEFAULT_NODE_LIST = DEFAULT_NODE_LIST;
    P2PParams::LOOP_DELAY = LOOP_DELAY;
    P2PParams::LOOP_DELAY_VARIABILITY = LOOP_DELAY_VARIABILITY;
    P2PParams::NODE_NOT_ACCEPTING_CHECK_DELAY = NODE_NOT_ACCEPTING_CHECK_DELAY;
    P2PParams::WALK_LINKS_EXPIRE_TIME = WALK_LINKS_EXPIRE_TIME;
    P2PParams::AUTH_KEY_EXPIRY = AUTH_KEY_EXPIRY;
    P2PParams::TGT_NUM_LINKS = TGT_NUM_LINKS;
    P2PParams::TGT_NUM_NONE_P2P_LINKS = TGT_NUM_NONE_P2P_LINKS;
    P2PParams::MIN_NUM_CONNECTIONS = MIN_NUM_CONNECTIONS;
    P2PParams::METRICS_DELAY = METRICS_DELAY;
    P2PParams::SAVE_DATA_DELAY = SAVE_DATA_DELAY;
    P2PParams::HASH_RATE_UPDATE_DELAY = HASH_RATE_UPDATE_DELAY;
    P2PParams::LOG_LEVEL = LOG_LEVEL;
}

} } } } } }