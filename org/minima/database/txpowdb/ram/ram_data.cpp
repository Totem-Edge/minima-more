#include "org/minima/database/txpowdb/ram/ram_data.hpp"

#include <chrono>

namespace org {
namespace minima {
namespace database {
namespace txpowdb {
namespace ram {

namespace {
inline std::int64_t currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
} // anonymous namespace

RamData::RamData(std::shared_ptr<org::minima::objects::TxPoW> zTxPoW)
    : mTxPoW(std::move(zTxPoW)),
      mLastAccess(currentTimeMillis()),
      mIsOnMainChain(false),
      mIsInCascade(false) {
}

std::shared_ptr<org::minima::objects::TxPoW> RamData::getTxPoW() const {
    return mTxPoW;
}

void RamData::updateLastAccess() {
    mLastAccess = currentTimeMillis();
}

std::int64_t RamData::getLastAccess() const {
    return mLastAccess;
}

void RamData::setOnMainChain(bool zOnChain) {
    mIsOnMainChain = zOnChain;
}

bool RamData::isOnMainChain() const {
    return mIsOnMainChain;
}

void RamData::setInCascade(bool zCascader) {
    mIsInCascade = zCascader;
}

bool RamData::isInCascade() const {
    return mIsInCascade;
}

} // namespace ram
} // namespace txpowdb
} // namespace database
} // namespace minima
} // namespace org