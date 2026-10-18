#include "org/minima/system/params/test_params.hpp"

#include "org/minima/system/params/global_params.hpp"

namespace org {
namespace minima {
namespace system {
namespace params {

// Static member definitions

std::string TestParams::MINIMA_VERSION = std::string(); // Will be computed in setTestParams()

org::minima::objects::base::MiniNumber TestParams::MINIMA_BLOCK_SPEED("0.05");

org::minima::objects::base::MiniNumber TestParams::MINIMA_BLOCKS_SPEED_CALC(16);

org::minima::objects::base::MiniNumber TestParams::MINIMA_CONFIRM_DEPTH("3");

org::minima::objects::base::MiniNumber TestParams::MINIMA_CASCADE_FREQUENCY(3);

org::minima::objects::base::MiniNumber TestParams::MINIMA_CASCADE_START_DEPTH(32);

int TestParams::MINIMA_CASCADE_LEVEL_NODES = 4;

int TestParams::MINIMA_CASCADE_LEVELS = 32;

org::minima::objects::base::MiniNumber TestParams::MINIMA_MMR_PROOF_HISTORY(8);

int TestParams::MEDIAN_BLOCK_CALC = 8;

void TestParams::setTestParams() {
    // Compute MINIMA_VERSION based on current GlobalParams (mirrors Java: GlobalParams.MINIMA_VERSION + "-TEST")
    TestParams::MINIMA_VERSION = org::minima::system::params::GlobalParams::MINIMA_VERSION + "-TEST";

    // Copy values into GlobalParams
    org::minima::system::params::GlobalParams::MINIMA_BLOCK_SPEED           = TestParams::MINIMA_BLOCK_SPEED;
    org::minima::system::params::GlobalParams::MINIMA_BLOCKS_SPEED_CALC     = TestParams::MINIMA_BLOCKS_SPEED_CALC;
    org::minima::system::params::GlobalParams::MINIMA_CASCADE_FREQUENCY     = TestParams::MINIMA_CASCADE_FREQUENCY;
    org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVEL_NODES   = TestParams::MINIMA_CASCADE_LEVEL_NODES;
    org::minima::system::params::GlobalParams::MINIMA_CASCADE_LEVELS        = TestParams::MINIMA_CASCADE_LEVELS;
    org::minima::system::params::GlobalParams::MINIMA_CASCADE_START_DEPTH   = TestParams::MINIMA_CASCADE_START_DEPTH;
    org::minima::system::params::GlobalParams::MINIMA_CONFIRM_DEPTH         = TestParams::MINIMA_CONFIRM_DEPTH;
    org::minima::system::params::GlobalParams::MINIMA_MMR_PROOF_HISTORY     = TestParams::MINIMA_MMR_PROOF_HISTORY;
    org::minima::system::params::GlobalParams::MINIMA_VERSION               = TestParams::MINIMA_VERSION;
    org::minima::system::params::GlobalParams::MEDIAN_BLOCK_CALC            = TestParams::MEDIAN_BLOCK_CALC;
}

} // namespace params
} // namespace system
} // namespace minima
} // namespace org