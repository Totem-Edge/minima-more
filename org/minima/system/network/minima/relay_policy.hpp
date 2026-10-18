#pragma once

#include <cstdint>

// Namespaced forward declaration to avoid heavy includes in the header
namespace org { namespace minima { namespace objects {
class TxPoW;
} } }

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

class RelayPolicy {
public:
    // Check maximum number of outputs in main and burn transactions
    static bool checkMaxCoinOutputNumber(org::minima::objects::TxPoW& zTxPow);

    // Check maximum total size of stored states across main and burn outputs
    static bool checkMaxStateStoreSize(org::minima::objects::TxPoW& zTxPow, std::int64_t zMaxSize);

    // Check all relay policies
    static bool checkAllPolicies(org::minima::objects::TxPoW& zTxPow, std::int64_t zMaxSize);
};

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org