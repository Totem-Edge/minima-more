#include "org/minima/system/commands/txn/txnlock.hpp"

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_number.hpp"

#include <thread>
#include <chrono>
#include <mutex>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

// Initialize static members
std::atomic<bool> txnlock::mLocked{false};

txnlock::txnlock()
    : org::minima::system::commands::Command(
          "txnlock",
          "(action) (timeout) (unlockdelay) - Gain a lock. Stops multiple txnfunctions occuring simultaneously") {
}

std::string txnlock::getFullHelp() const {
    // Preserve the exact string content from Java (including the "\t" sequence)
    return "\txnlock\n"
           "\n"
           "When creating multiple transactions asynchronously you can ensure each is created one at a time.\n"
           "\n"
           "action:\n"
           "    lock - Gain a lock\n"
           "    unlock - Release lock\n"
           "    list/blank - Are we locked\n"
           "\n"
           "timeout:\n"
           "    How long to wait for the lock\n"
           "\n"
           "unlockdelay:\n"
           "    Wait this amount of milli after unlock.\n"
           "\n"
           "Examples:\n"
           "\n"
           "txnlock\n"
           "\n"
           "txnlock action:lock timeout:10000\n"
           "\n"
           "txnlock action:unlock unlockdelay:5000\n"
           "\n";
}

std::vector<std::string> txnlock::getValidParams() const {
    return std::vector<std::string>{ "action", "timeout", "unlockdelay" };
}

bool txnlock::lockFunction(bool zEnable) {
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lk(s_mutex);

    if (zEnable) {
        if (!mLocked.load(std::memory_order_relaxed)) {
            mLocked.store(true, std::memory_order_relaxed);
            return true;
        }
    } else {
        mLocked.store(false, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool txnlock::getLock(long long zTimeout) {
    const long long delay = 100;
    long long counter = 0;

    while (!lockFunction(true)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        counter += delay;

        if (zTimeout != 0) {
            if (counter > zTimeout) {
                return false;
            }
        }
    }
    return true;
}

void txnlock::unlock() {
    lockFunction(false);
}

std::unique_ptr<org::minima::utils::json::JSONObject> txnlock::runCommand() {
    auto ret = getJSONReply();

    // Lock or unlock
    std::string action = getParam("action", "list");

    // 10 second default timer
    long long timeout = getNumberParam("timeout", org::minima::objects::base::MiniNumber(20000))->getAsLong();
    long long unlockdelay = getNumberParam("unlockdelay", org::minima::objects::base::MiniNumber::ZERO())->getAsLong();

    org::minima::utils::json::JSONObject resp;
    if (action == "lock") {
        bool success = getLock(timeout);
        resp.put("success", success);
        resp.put("locked", true);
    } else if (action == "unlock") {
        if (unlockdelay != 0) {
#ifdef _WIN32
            std::this_thread::sleep_for(std::chrono::milliseconds(unlockdelay));
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(unlockdelay));
#endif
        }
        unlock();

        resp.put("success", true);
        resp.put("locked", false);
    } else {
        resp.put("locked", mLocked.load(std::memory_order_relaxed));
    }

    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* txnlock::getFunction() {
    return new txnlock();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org