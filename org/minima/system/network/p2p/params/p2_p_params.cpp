#include "org/minima/system/network/p2p/params/p2_p_params.hpp"

namespace org { namespace minima { namespace system { namespace network { namespace p2p { namespace params {

// Static member definitions with Java-equivalent default values

std::string P2PParams::VERSION = "1.1";

org::minima::system::network::p2p::P2PFunctions::Level P2PParams::LOG_LEVEL =
    org::minima::system::network::p2p::P2PFunctions::Level::NODE_RUNNER_MSG;

int P2PParams::PEERS_LIST_SIZE = 50;

int P2PParams::TGT_NUM_LINKS = 5;

int P2PParams::TGT_NUM_NONE_P2P_LINKS = 100;

int P2PParams::MIN_NUM_CONNECTIONS = 2;

int P2PParams::LOOP_DELAY = 600000;

int P2PParams::HASH_RATE_UPDATE_DELAY = 12 * 60 * 60 * 1000;

int P2PParams::LOOP_DELAY_VARIABILITY = 30000;

int P2PParams::NODE_NOT_ACCEPTING_CHECK_DELAY = 3600000;

int P2PParams::SAVE_DATA_DELAY = 1000 * 60 * 10;

int P2PParams::WALK_LINKS_EXPIRE_TIME = 10000;

int P2PParams::AUTH_KEY_EXPIRY = 300000;

int P2PParams::METRICS_DELAY = 600000;

std::vector<InetSocketAddress> P2PParams::DEFAULT_NODE_LIST = {};

} } } } } }