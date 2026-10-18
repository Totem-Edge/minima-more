#include "org/minima/objects/pulse.hpp"

#include <exception>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/database/minima_d_b.hpp"
#include "org/minima/database/txpowtree/tx_pow_tree.hpp"

namespace org {
namespace minima {
namespace objects {

using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::MinimaLogger;

// Define static version constant
MiniNumber Pulse::PULSE_VERSION = MiniNumber::ONE();

Pulse::Pulse()
    : mBlockList() {
}

void Pulse::setBlockList(const std::vector<MiniData>& zBlockList) {
    mBlockList = zBlockList;
}

const std::vector<MiniData>& Pulse::getBlockList() const {
    return mBlockList;
}

void Pulse::writeDataStream(std::ostream& out) {
    // Write the Version
    PULSE_VERSION.writeDataStream(out);

    // Write size as MiniNumber
    MiniNumber::WriteToStream(out, static_cast<int>(mBlockList.size()));

    // Write all blocks
    for (MiniData& block : mBlockList) {
        block.writeDataStream(out);
    }
}

void Pulse::readDataStream(std::istream& in) {
    mBlockList.clear();

    // Version may change in future
    MiniNumber version = MiniNumber::ReadFromStream(in);
    if (!version.isEqual(MiniNumber::ONE())) {
        MinimaLogger::log(std::string("UNKNOWN PULSE Version ") + version.toString());
        return;
    }

    int len = MiniNumber::ReadFromStream(in).getAsInt();
    if (len < 0) {
        // Defensive: negative lengths are invalid; mimic Java behavior by not throwing here.
        return;
    }

    mBlockList.reserve(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i) {
        MiniData block = MiniData::ReadFromStream(in);
        mBlockList.emplace_back(std::move(block));
    }
}

Pulse Pulse::ReadFromStream(std::istream& in) {
    Pulse pp;
    pp.readDataStream(in);
    return pp;
}

Pulse Pulse::createPulse() {
    using org::minima::database::MinimaDB;

    // New Pulse
    Pulse pulse;

    // Lock - don't want it changing half way through
    MinimaDB::getDB()->readLock(true);

    try {
        // Get the pulse list from the tree
        const auto& plist = MinimaDB::getDB()->getTxPoWTree().getPulseList();
        
        // Set the block list on the pulse
        pulse.setBlockList(plist);
        
    } catch (const std::exception& exc) {
        MinimaLogger::log(exc);
    } catch (...) {
        MinimaLogger::log(std::string("UNKNOWN EXCEPTION in Pulse::createPulse"));
    }

    // Unlock
    MinimaDB::getDB()->readLock(false);

    return pulse;
}

} // namespace objects
} // namespace minima
} // namespace org