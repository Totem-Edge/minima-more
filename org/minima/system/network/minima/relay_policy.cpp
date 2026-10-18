#include "org/minima/system/network/minima/relay_policy.hpp"

#include <string>
#include <cstdint>

#include "org/minima/objects/tx_po_w.hpp"

#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/coin.hpp"
#include "org/minima/system/params/general_params.hpp"
#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace system {
namespace network {
namespace minima {

using org::minima::objects::TxPoW;
using org::minima::system::params::GeneralParams;
using org::minima::utils::MinimaLogger;

// Helper for unique_ptr<Coin> elements returned by Transaction::getAllOutputs().
static inline bool coinStoreState(const std::unique_ptr<org::minima::objects::Coin>& c) {
    return c && c->storeState();
}

bool RelayPolicy::checkMaxCoinOutputNumber(TxPoW& zTxPow) {
    // Main transaction outputs
    {
        const auto& outputs = zTxPow.getTransaction().getAllOutputs();
        int outsize = static_cast<int>(outputs.size());
        if (outsize > GeneralParams::MAX_RELAY_OUTPUTCOINS) {
            MinimaLogger::log(std::string("MAX Coin outpouts in Transaction @ ") + zTxPow.getTxPoWID());
            return false;
        }
    }

    // Burn transaction outputs
    {
        const auto& outputs = zTxPow.getBurnTransaction().getAllOutputs();
        int outsize = static_cast<int>(outputs.size());
        if (outsize > GeneralParams::MAX_RELAY_OUTPUTCOINS) {
            MinimaLogger::log(std::string("MAX Coin outpouts in Burn Transaction @ ") + zTxPow.getTxPoWID());
            return false;
        }
    }

    return true;
}

bool RelayPolicy::checkMaxStateStoreSize(TxPoW& zTxPow, std::int64_t zMaxSize) {
    // Count outputs that store state - main
    int countertxn = 0;
    {
        const auto& outputs = zTxPow.getTransaction().getAllOutputs();
        for (const auto& out : outputs) {
            if (coinStoreState(out)) {
                ++countertxn;
            }
        }
    }

    // State size - main
    std::int64_t statesizetxn = static_cast<std::int64_t>(zTxPow.getTransaction().calculateStateSize());
    std::int64_t totaltxn     = statesizetxn * static_cast<std::int64_t>(countertxn);

    // Count outputs that store state - burn
    int counterburn = 0;
    {
        const auto& outputs = zTxPow.getBurnTransaction().getAllOutputs();
        for (const auto& out : outputs) {
            if (coinStoreState(out)) {
                ++counterburn;
            }
        }
    }

    // State size - burn
    std::int64_t statesizeburn = static_cast<std::int64_t>(zTxPow.getBurnTransaction().calculateStateSize());
    std::int64_t totalburn     = statesizeburn * static_cast<std::int64_t>(counterburn);

    // Total
    std::int64_t total = totaltxn + totalburn;

    if (total > zMaxSize) {
        MinimaLogger::log(
            std::string("TXPoW exceeds maximum state store policy")
            + " coinstxn:"     + std::to_string(countertxn)
            + " coinsburn:"    + std::to_string(counterburn)
            + " statesizetxn:" + std::to_string(statesizetxn)
            + " statesizeburn:" + std::to_string(statesizeburn)
            + " for " + std::to_string(total) + " / " + std::to_string(zMaxSize)
        );
        return false;
    }

    return true;
}

bool RelayPolicy::checkAllPolicies(TxPoW& zTxPow, std::int64_t zMaxSize) {
    if (!checkMaxCoinOutputNumber(zTxPow)) {
        return false;
    }

    if (!checkMaxStateStoreSize(zTxPow, zMaxSize)) {
        MinimaLogger::log("Relay Policy FAIL - not forwarding txn..");
        return false;
    }

    return true;
}

} // namespace minima
} // namespace network
} // namespace system
} // namespace minima
} // namespace org