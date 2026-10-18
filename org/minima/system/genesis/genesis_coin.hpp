#pragma once

#include "org/minima/objects/coin.hpp"

namespace org { namespace minima { namespace objects { namespace base {
    class MiniData;
} } } }

namespace org {
namespace minima {
namespace system {
namespace genesis {

class GenesisCoin final : public org::minima::objects::Coin {
public:
    // Static constant matching Java's public static final MiniData GENESIS_COINID
    static const org::minima::objects::base::MiniData GENESIS_COINID;

    // Constructor replicating Java logic
    GenesisCoin();

    // Default destructor
    ~GenesisCoin() override = default;

    // Delete copy operations to match typical ownership semantics
    GenesisCoin(const GenesisCoin&) = delete;
    GenesisCoin& operator=(const GenesisCoin&) = delete;

    // Allow moves
    GenesisCoin(GenesisCoin&&) noexcept = default;
    GenesisCoin& operator=(GenesisCoin&&) noexcept = default;
};

} // namespace genesis
} // namespace system
} // namespace minima
} // namespace org