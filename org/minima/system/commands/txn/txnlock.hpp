#pragma once

#include "org/minima/system/commands/command.hpp"

#include <string>
#include <vector>
#include <atomic>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnlock : public org::minima::system::commands::Command {
public:
    txnlock();

    // Help and params (hide base implementation)
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams() const;

    // Locking API
    static bool lockFunction(bool zEnable);
    bool getLock(long long zTimeout);
    void unlock();

    // Command interface
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;
    Command* getFunction() override;

    // Public static lock flag (matches Java public static boolean)
    static std::atomic<bool> mLocked;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org