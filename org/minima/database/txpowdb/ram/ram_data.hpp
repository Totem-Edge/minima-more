#pragma once
#ifndef ORG_MINIMA_DATABASE_TXPOWDB_RAM_RAM_DATA_HPP
#define ORG_MINIMA_DATABASE_TXPOWDB_RAM_RAM_DATA_HPP

#include <memory>
#include <cstdint>

namespace org { namespace minima { namespace objects {
class TxPoW; // forward declaration
} } }

namespace org {
namespace minima {
namespace database {
namespace txpowdb {
namespace ram {

class RamData {
public:
    // Constructor
    explicit RamData(std::shared_ptr<org::minima::objects::TxPoW> zTxPoW);

    // Defaulted special members are fine for shared_ptr
    RamData(const RamData&) = default;
    RamData& operator=(const RamData&) = default;
    RamData(RamData&&) noexcept = default;
    RamData& operator=(RamData&&) noexcept = default;
    ~RamData() = default;

    // Accessors - return shared_ptr to preserve Java-like reference/null semantics
    std::shared_ptr<org::minima::objects::TxPoW> getTxPoW() const;

    void updateLastAccess();
    std::int64_t getLastAccess() const;

    void setOnMainChain(bool zOnChain);
    bool isOnMainChain() const;

    void setInCascade(bool zCascader);
    bool isInCascade() const;

private:
    std::shared_ptr<org::minima::objects::TxPoW> mTxPoW;
    std::int64_t mLastAccess;
    bool mIsOnMainChain = false;
    bool mIsInCascade   = false;
};

} // namespace ram
} // namespace txpowdb
} // namespace database
} // namespace minima
} // namespace org

#endif // ORG_MINIMA_DATABASE_TXPOWDB_RAM_RAM_DATA_HPP