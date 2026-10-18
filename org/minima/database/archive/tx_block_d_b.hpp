#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace org { namespace minima { namespace objects { class TxBlock; } } }
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }

namespace org {
namespace minima {
namespace database {
namespace archive {

class TxBlockDB {
public:
    TxBlockDB();
    ~TxBlockDB() = default;

    // Non-copyable and non-movable (std::mutex member makes moves implicitly deleted)
    TxBlockDB(const TxBlockDB&) = delete;
    TxBlockDB& operator=(const TxBlockDB&) = delete;
    TxBlockDB(TxBlockDB&&) = delete;
    TxBlockDB& operator=(TxBlockDB&&) = delete;

    // Add a block (stores/overwrites by TxPoWID)
    void addTxBlock(const std::shared_ptr<org::minima::objects::TxBlock>& zTxBlock);

    // Find by TxPoW ID - returns nullptr if not found
    std::shared_ptr<org::minima::objects::TxBlock> findTxBlock(const std::string& zTxPowID);

    // Get children of a given TxPoW ID
    std::vector<std::shared_ptr<org::minima::objects::TxBlock>> getChildBlocks(const std::string& zTxPowID);

    // Clear all
    void clearAll();

    // Keep only blocks with blockNumber >= zMinBlock
    void clearOld(const org::minima::objects::base::MiniNumber& zMinBlock);

private:
    std::unordered_map<std::string, std::shared_ptr<org::minima::objects::TxBlock>> mTxBlockDB;
    std::mutex mMutex;
};

} // namespace archive
} // namespace database
} // namespace minima
} // namespace org