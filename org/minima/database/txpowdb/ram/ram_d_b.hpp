#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <shared_mutex>

// Namespaced Forward declarations for project classes (Rule 2 & 4)
namespace org { namespace minima { namespace objects { class TxPoW; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniData; } } } }
namespace org { namespace minima { namespace objects { class CoinProof; } } }

//
// ### FIX 1 ###
// Removed the include for ram_data.hpp.
//

// Java Package to C++ Namespace Mapping for RamDB: org.minima.database.txpowdb.ram
namespace org { namespace minima { namespace database { namespace txpowdb { namespace ram {

//
// ### FIX 2 ###
// Added a forward declaration for RamData instead.
//
class RamData;

/**
 * RamDB - in-memory pool of TxPoW with aging and mempool utilities.
 * Mirrors org.minima.database.txpowdb.ram.RamDB
 */
class RamDB {
public:
    // How long does data remain in RAM DB in milliseconds
    std::int64_t MAX_TIME;

    RamDB();
    ~RamDB() = default;

    bool addTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW);

    // This now correctly uses the forward-declared RamData
    std::unordered_map<std::string, std::shared_ptr<RamData>> getCompleteMemPool() const;

    bool exists(const std::string& zTxPoWID) const;

    std::shared_ptr<org::minima::objects::TxPoW> getTxPoW(const std::string& zTxPoWID);

    void remove(const std::string& zTxPoWID);

    void cleanDB();

    int getSize() const;

    void wipeRamDB();

    // MEMPOOL specific functions
    void clearMainChainTxns();
    void setOnMainChain(const std::string& zTxPoWID);
    
    std::vector<std::shared_ptr<org::minima::objects::TxPoW>> getAllUnusedTxns() const;
    void setInCascade(const std::string& zTxPoWID);

    bool checkForCoinID(const org::minima::objects::base::MiniData& zCoinID) const;

private:
    static std::int64_t currentTimeMillis();

    // Coin index helpers. mCoinToTxPoW maps an input CoinID to the mempool
    // TxPoW that references it, but only for transaction entries that are not
    // yet in the cascade (matching checkForCoinID's original filter). Kept in
    // sync on add/remove/clean/wipe and when a TxPoW enters the cascade.
    void indexTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW);
    void unindexTxPoW(const std::shared_ptr<org::minima::objects::TxPoW>& zTxPoW);
    void rebuildCoinIndex();

private:
    mutable std::shared_mutex mMutex;
    
    // This now correctly uses the forward-declared RamData
    std::unordered_map<std::string, std::shared_ptr<RamData>> mTxPoWDB;

    // coinID (0x..) -> TxPoW ID, for O(1) mempool coin lookups
    std::unordered_map<std::string, std::string> mCoinToTxPoW;
};

} } } } } // end namespace org::minima::database::txpowdb::ram