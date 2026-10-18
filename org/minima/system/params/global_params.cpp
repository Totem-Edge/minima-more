#include "org/minima/system/params/global_params.hpp"

namespace org {
namespace minima {
namespace system {
namespace params {

// Versioning
std::string GlobalParams::MINIMA_BASE_VERSION = "1.0";
std::string GlobalParams::MINIMA_BUILD_NUMBER = "100";
std::string GlobalParams::MINIMA_VERSION = GlobalParams::MINIMA_BASE_VERSION + std::string(".") + GlobalParams::MINIMA_BUILD_NUMBER;

// Micro build
std::string GlobalParams::MINIMA_MICRO_BUILD = "0";
std::string GlobalParams::getFullMicroVersion() {
    return MINIMA_VERSION + std::string(".") + MINIMA_MICRO_BUILD;
}

// Parameters
org::minima::objects::base::MiniNumber GlobalParams::MINIMA_BLOCK_SPEED("0.02");
org::minima::objects::base::MiniNumber GlobalParams::MINIMA_BLOCKS_SPEED_CALC(256);
org::minima::objects::base::MiniNumber GlobalParams::MINIMA_CONFIRM_DEPTH("3");
org::minima::objects::base::MiniNumber GlobalParams::MINIMA_CASCADE_FREQUENCY(100);
org::minima::objects::base::MiniNumber GlobalParams::MINIMA_CASCADE_START_DEPTH(2048);
int GlobalParams::MINIMA_CASCADE_LEVEL_NODES = 256;
int GlobalParams::MINIMA_CASCADE_LEVELS = 32;
org::minima::objects::base::MiniNumber GlobalParams::MINIMA_MMR_PROOF_HISTORY(256);
int GlobalParams::MEDIAN_BLOCK_CALC = 64;

} // namespace params
} // namespace system
} // namespace minima
} // namespace org