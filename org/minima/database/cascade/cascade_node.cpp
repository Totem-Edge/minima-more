#include "org/minima/database/cascade/cascade_node.hpp"

#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include "org/minima/objects/tx_po_w.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace database {
namespace cascade {

using org::minima::objects::TxPoW;
using org::minima::objects::base::MiniNumber;
using org::minima::utils::MinimaLogger;

static std::string formatDateFromMillis(long long millis) {
    std::time_t tt = static_cast<std::time_t>(millis / 1000);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    // Locale-dependent human-readable timestamp
    oss << std::put_time(&tm, "%c");
    return oss.str();
}

// Destructor and move operations definitions (for unique_ptr to incomplete type)
CascadeNode::~CascadeNode() = default;
CascadeNode::CascadeNode(CascadeNode&&) noexcept = default;
CascadeNode& CascadeNode::operator=(CascadeNode&&) noexcept = default;

CascadeNode::CascadeNode() = default;

CascadeNode::CascadeNode(const TxPoW& zTxPoW) {
    // CRITICAL: Capture SuperLevel from the input BEFORE deepCopy/clearBody modifications [cite: 1]
    // Java: mSuperLevel = zTxPoW.getSuperLevel();
    mSuperLevel = zTxPoW.getSuperLevel();

    // Use a deep copy
    auto copy = zTxPoW.deepCopy();
    mTxPoW = std::move(copy);

    // Remove the body
    if (mTxPoW) {
        mTxPoW->clearBody();
    }

    // Zero level
    mCurrentLevel = 0;
}

TxPoW& CascadeNode::getTxPoW() {
    if (!mTxPoW) {
        throw std::runtime_error("CascadeNode::getTxPoW called with null TxPoW");
    }
    return *mTxPoW;
}

const TxPoW& CascadeNode::getTxPoW() const {
    if (!mTxPoW) {
        throw std::runtime_error("CascadeNode::getTxPoW (const) called with null TxPoW");
    }
    return *mTxPoW;
}

int CascadeNode::getLevel() const {
    return mCurrentLevel;
}

void CascadeNode::setLevel(int zLevel) {
    mCurrentLevel = zLevel;
}

int CascadeNode::getSuperLevel() const {
    // IMPORTANT: Return the stored mSuperLevel, not from TxPoW
    // This is the value captured from the original TxPoW before any modifications
    return mSuperLevel;
}

void CascadeNode::setParent(CascadeNode* zCascadeNode) {
    mParent = zCascadeNode;
}

CascadeNode* CascadeNode::getParent() const {
    return mParent;
}

std::string CascadeNode::getCurrentWeight() const {
    if (!mTxPoW) {
        return "0";
    }

    // Java Logic:
    // BigDecimal weight = getTxPoW().getWeight();
    // BigDecimal factor = BIGDECIMAL_TWO.pow(getLevel());
    // return weight.multiply(factor, MathContext.DECIMAL32); [cite: 1]

    // C++ Logic using MiniNumber:
    org::minima::objects::base::MiniNumber weight(getTxPoW().getWeight());
    org::minima::objects::base::MiniNumber factor = org::minima::objects::base::MiniNumber::TWO().pow(getLevel());
    org::minima::objects::base::MiniNumber result = weight.mult(factor);

    // Java's MathContext.DECIMAL32 implies 7 significant digits of precision
    return result.setSignificantDigits(7).toString();
}

std::string CascadeNode::toString() const {
    if (!mTxPoW) {
        return "[null/null] null @ null weight:0 @ null";
    }

    std::ostringstream ss;
    ss << "[" << getLevel() << "/" << getSuperLevel() << "] "
       << mTxPoW->getTxPoWID()
       << " @ " << mTxPoW->getBlockNumber().toString()
       << " weight:" << getCurrentWeight()
       << " @ " << formatDateFromMillis(mTxPoW->getTimeMilli().getAsLong());
    return ss.str();
}

void CascadeNode::writeDataStream(std::ostream& out) {
    if (!mTxPoW) {
        throw std::runtime_error("CascadeNode::writeDataStream - TxPoW is null");
    }
    // Serialize TxPoW then current level (as MiniNumber int utility)
    mTxPoW->writeDataStream(out);
    MiniNumber::WriteToStream(out, mCurrentLevel);
}

void CascadeNode::readDataStream(std::istream& in) {
    // Read TxPoW (now returns unique_ptr directly - no double move!)
    mTxPoW = TxPoW::ReadFromStream(in);

    // Read current level
    mCurrentLevel = MiniNumber::ReadFromStream(in).getAsInt();
    
    // CRITICAL: Derive super level from TxPoW
    // This must match the behavior in the constructor where we store it from the original
    if (!mTxPoW) {
        throw std::runtime_error("CascadeNode::readDataStream - Failed to read TxPoW");
    }
    
    // Store the SuperLevel from the deserialized TxPoW
    mSuperLevel = mTxPoW->getSuperLevel();
    
    // MinimaLogger::log("DEBUG CascadeNode::readDataStream: Loaded node");
    // MinimaLogger::log("  Block: " + mTxPoW->getBlockNumber().toString());
    // MinimaLogger::log("  SuperLevel: " + std::to_string(mSuperLevel));
    // MinimaLogger::log("  CurrentLevel: " + std::to_string(mCurrentLevel));

    // Parent is not serialized; remains as-is (likely nullptr)
}

std::unique_ptr<CascadeNode> CascadeNode::ReadFromStream(std::istream& in) {
    auto casc = std::make_unique<CascadeNode>();
    casc->readDataStream(in);
    return casc;
}

} // namespace cascade
} // namespace database
} // namespace minima
} // namespace org