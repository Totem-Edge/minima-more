#pragma once

#include <string>

#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace system {
namespace params {

class TestParams {
public:
    // Which Version
    static std::string MINIMA_VERSION;

    // Speed in blocks per second
    static org::minima::objects::base::MiniNumber MINIMA_BLOCK_SPEED;

    // When checking speed and average difficulty only look at this many blocks back
    static org::minima::objects::base::MiniNumber MINIMA_BLOCKS_SPEED_CALC;

    // How deep before we think confirmed
    static org::minima::objects::base::MiniNumber MINIMA_CONFIRM_DEPTH;

    // How often do we cascade the chain
    static org::minima::objects::base::MiniNumber MINIMA_CASCADE_FREQUENCY;

    // Depth before we cascade
    static org::minima::objects::base::MiniNumber MINIMA_CASCADE_START_DEPTH;

    // Number of blocks at each cascade level
    static int MINIMA_CASCADE_LEVEL_NODES;

    // How Many Cascade Levels
    static int MINIMA_CASCADE_LEVELS;

    // Max Proof History - how far back to use a proof of coin
    static org::minima::objects::base::MiniNumber MINIMA_MMR_PROOF_HISTORY;

    // The MEDIAN time block is taken from this many blocks back
    static int MEDIAN_BLOCK_CALC;

    // Set these as the GlobalParams..
    static void setTestParams();
};

} // namespace params
} // namespace system
} // namespace minima
} // namespace org