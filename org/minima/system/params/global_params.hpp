#pragma once

#include <string>
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace system {
namespace params {

class GlobalParams {
public:
    // Which Version of Minima are we running
    static std::string MINIMA_BASE_VERSION;
    static std::string MINIMA_BUILD_NUMBER;
    static std::string MINIMA_VERSION;

    // The MICRO build number
    static std::string MINIMA_MICRO_BUILD;
    static std::string getFullMicroVersion();

    // Speed in blocks per second.. 0.02 = 50 second block time
    static org::minima::objects::base::MiniNumber MINIMA_BLOCK_SPEED;

    // When checking speed and difficulty only look at this many blocks back
    static org::minima::objects::base::MiniNumber MINIMA_BLOCKS_SPEED_CALC;

    // How deep before we think confirmed..
    static org::minima::objects::base::MiniNumber MINIMA_CONFIRM_DEPTH;

    // How often do we cascade the chain
    static org::minima::objects::base::MiniNumber MINIMA_CASCADE_FREQUENCY;

    // Depth before we cascade..
    static org::minima::objects::base::MiniNumber MINIMA_CASCADE_START_DEPTH;

    // Number of blocks at each cascade level 
    static int MINIMA_CASCADE_LEVEL_NODES;

    // How Many Cascade Levels
    static int MINIMA_CASCADE_LEVELS;

    // Max Proof History - how far back to use a proof of coin..
    // If there is a re-org of more than this the proof will be invalid 
    static org::minima::objects::base::MiniNumber MINIMA_MMR_PROOF_HISTORY;

    // The MEDIAN time block is taken from this many blocks back
    // When calculating the Difficulty of a block ( both from the tip and the previous block )
    // This smooths out the time fluctuations for different blocks and removes incorrect times.
    // 64 blocks means the block 1/2 hour ago.
    static int MEDIAN_BLOCK_CALC;
};

} // namespace params
} // namespace system
} // namespace minima
} // namespace org