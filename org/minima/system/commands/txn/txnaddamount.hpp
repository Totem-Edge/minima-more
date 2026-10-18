#pragma once

#include <string>
#include <vector>

#include "org/minima/system/commands/command.hpp"

// Forward declarations to avoid heavy includes in header (Pitfall 4)
namespace org { namespace minima { namespace utils { namespace json { class JSONObject; } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

class txnaddamount : public org::minima::system::commands::Command {
public:
    txnaddamount();
    virtual ~txnaddamount() = default;

    // Help/params
    std::string getFullHelp() const override;
    std::vector<std::string> getValidParams();

    // Core execution
    std::unique_ptr<org::minima::utils::json::JSONObject> runCommand() override;

    // Return a new instance
    org::minima::system::commands::Command* getFunction() override;

    // Static coin lock management (functional parity with Java)
    static void enableCoinLock(bool zCoinLockEnabled);
    static bool isCoinLockEnabled();
    static void addCoinLock(const std::string& zCoinID);
    static bool isCoinLocked(const std::string& zCoinID);
    static void clearCoinLocked();

private:
    // Static state (no synchronization to match Java semantics)
    static std::vector<std::string> s_LOCKED_COINS;
    static bool s_LOCKED_COINS_ENABLED;
};

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org