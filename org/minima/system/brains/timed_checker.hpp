#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <cstdint>

// Namespaced forward declarations for project classes (Rule 10)
namespace org { namespace minima { namespace database { namespace txpowtree { class TxPoWTreeNode; } } } }
namespace org { namespace minima { namespace objects { class TxPoW; } } }

namespace org {
namespace minima {
namespace system {
namespace brains {

class TimedChecker {
public:
    static constexpr std::int64_t MAX_CHECKTIME = 120000; // 120 seconds

    TimedChecker();
    ~TimedChecker();

    // Move operations
    TimedChecker(TimedChecker&&) noexcept;
    TimedChecker& operator=(TimedChecker&&) noexcept;

    // Delete copy operations
    TimedChecker(const TimedChecker&) = delete;
    TimedChecker& operator=(const TimedChecker&) = delete;

    bool checkTxPoWBlock(org::minima::database::txpowtree::TxPoWTreeNode* zParentNode,
                         org::minima::objects::TxPoW& zTxPoW,
                         std::vector<org::minima::objects::TxPoW*>& zTransactions);

private:
    struct SharedState {
        std::atomic<bool> finished{false};
        std::atomic<bool> valid{false};
    };

    std::shared_ptr<SharedState> mState;
    std::thread mCheckerThread;
};

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org