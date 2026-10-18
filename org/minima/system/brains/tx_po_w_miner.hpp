#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <mutex>           // For std::mutex
#include <unordered_set>   // For std::unordered_set

#include "org/minima/utils/messages/message_processor.hpp"

// Namespaced forward declarations to avoid heavy includes (PITFALL 4)
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { class TxHeader; } } }
namespace org { namespace minima { namespace objects { class Transaction; } } }
namespace org { namespace minima { namespace objects { class Witness; } } }
namespace org { namespace minima { namespace objects { class Coin; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; class MiniNumber; } } } }
namespace org { namespace minima { namespace utils { namespace messages { class Message; class TimerMessage; } } } }

namespace org {
namespace minima {
namespace system {
namespace brains {

class TxPoWMiner : public org::minima::utils::messages::MessageProcessor {
public:
    // Message type constants
    inline static const char* TXPOWMINER_MINETXPOW   = "TXPOWMINER_MINETXPOW";
    inline static const char* TXPOWMINER_MINEPULSE   = "TXPOWMINER_MINEPULSE";
    inline static const char* TXPOWMINER_TXBLOCKMINER= "TXPOWMINER_TXBLOCKMINER";

    TxPoWMiner();

    // Start async mining for a given TxPoW
    void mineTxPoWAsync(org::minima::objects::TxPoW& zTxPoW);

    // Check if a coin is currently being mined
    bool checkForMiningCoin(const std::string& zCoinID) const;

    // Mine a TxPoW with a time limit
    bool MineMaxTxPoW(org::minima::objects::TxPoW& zTxPoW, long long zTimeLimit);
    bool MineMaxTxPoW(org::minima::objects::TxPoW& zTxPoW, long long zTimeLimit, bool zPost);

    // Hashing speed calculations
    static org::minima::objects::base::MiniNumber calculateHashRateOld(org::minima::objects::base::MiniNumber zHashes);
    static org::minima::objects::base::MiniNumber calculateHashSpeed(const org::minima::objects::base::MiniNumber& zHashes);

protected:
    void processMessage(org::minima::utils::messages::Message& zMessage) override;

private:
    // Helpers to track coins being mined
    void addMiningCoins(org::minima::objects::TxPoW& zTxPoW);
    void removeMiningCoins(org::minima::objects::TxPoW& zTxPoW);

    // Best-effort fallback for automine timer due to lack of accessor in Main.hpp
    long long getAutomineTimerMillis() const;

    // Static START_NONCE_BYTES value
    static org::minima::objects::base::MiniNumber START_NONCE_BYTES;

    // Currently mining coin list
    mutable std::mutex mMiningCoinsMutex;
    std::unordered_set<std::string> mMiningCoins;
};

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org