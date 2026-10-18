#include "org/minima/system/brains/timed_checker.hpp"

#include <chrono>
#include <thread>
#include <sstream>
#include <exception>

#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/system/params/general_params.hpp"

// Forward declaration of TxPoWChecker with the static method used here.
namespace org {
namespace minima {
namespace system {
namespace brains {

class TxPoWChecker {
public:
    static bool checkTxPoWBlock(org::minima::database::txpowtree::TxPoWTreeNode* zParentNode,
                                org::minima::objects::TxPoW& zTxPoW, // <--- FIX: Was TxPoW*
                                std::vector<org::minima::objects::TxPoW*>& zTransactions);
};

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org

namespace org {
namespace minima {
namespace system {
namespace brains {

TimedChecker::TimedChecker()
    : mState(std::make_shared<SharedState>()) {}

TimedChecker::~TimedChecker() {
    // Ensure no std::terminate on destruction if thread still joinable
    if (mCheckerThread.joinable()) {
        // No portable interruption in C++17; detach to mimic Java's interrupt semantics
        mCheckerThread.detach();
    }
}

TimedChecker::TimedChecker(TimedChecker&& other) noexcept
    : mState(std::move(other.mState))
    , mCheckerThread(std::move(other.mCheckerThread)) {}

TimedChecker& TimedChecker::operator=(TimedChecker&& other) noexcept {
    if (this != &other) {
        if (mCheckerThread.joinable()) {
            mCheckerThread.detach();
        }
        mState = std::move(other.mState);
        mCheckerThread = std::move(other.mCheckerThread);
    }
    return *this;
}

bool TimedChecker::checkTxPoWBlock(org::minima::database::txpowtree::TxPoWTreeNode* zParentNode,
                                 org::minima::objects::TxPoW& zTxPoW,
                                 std::vector<org::minima::objects::TxPoW*>& zTransactions) {
    using namespace std::chrono;

    const auto wall_start = system_clock::now();
    auto start = steady_clock::now();
    milliseconds elapsed_ms{0};

    try {
        // Start a thread that does the checking
        auto state = mState;
        auto txns_copy = zTransactions; // copy vector of pointers (borrowed objects)

        mCheckerThread = std::thread([state, zParentNode, pTxPoW = &zTxPoW, txns_copy]() mutable {
            try {
                bool res = TxPoWChecker::checkTxPoWBlock(zParentNode, *pTxPoW, txns_copy);
                state->valid.store(res, std::memory_order_release);
            } catch (const std::exception& e) {
                org::minima::utils::MinimaLogger::log(std::string("Block failed to process : ") + e.what());
                state->valid.store(false, std::memory_order_release);
            } catch (...) {
                org::minima::utils::MinimaLogger::log(std::string("Block failed to process : unknown exception"));
                state->valid.store(false, std::memory_order_release);
            }
            state->finished.store(true, std::memory_order_release);
        });

        // Now check, waiting up to MAX_CHECKTIME
        while (elapsed_ms.count() < MAX_CHECKTIME) {
            std::this_thread::sleep_for(milliseconds(50));

            if (mState->finished.load(std::memory_order_acquire)) {
                break;
            }

            elapsed_ms = duration_cast<milliseconds>(steady_clock::now() - start);
        }

        // "Interrupt" the thread: in C++17 there's no interruption; do not block (no join), detach instead
        if (mCheckerThread.joinable()) {
            mCheckerThread.detach();
        }

    } catch (const std::exception& e) {
        org::minima::utils::MinimaLogger::log(e);
        mState->valid.store(false, std::memory_order_release);
    } catch (...) {
        org::minima::utils::MinimaLogger::log(std::string("Unknown exception in TimedChecker"));
        mState->valid.store(false, std::memory_order_release);
    }

    // Are we logging this
    if (org::minima::system::params::GeneralParams::BLOCK_LOGS) {
        auto wall_diff_ms = duration_cast<milliseconds>(system_clock::now() - wall_start).count();

        std::ostringstream oss;
        oss << "[VALID:" << (mState->valid.load(std::memory_order_acquire) ? "true" : "false")
            << "] Block checker time : " << wall_diff_ms << "ms";

        org::minima::utils::MinimaLogger::log(oss.str());
    }

    return mState->valid.load(std::memory_order_acquire);
}

} // namespace brains
} // namespace system
} // namespace minima
} // namespace org