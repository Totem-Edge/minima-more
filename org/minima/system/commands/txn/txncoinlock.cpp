#include "org/minima/system/commands/txn/txncoinlock.hpp"

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include <string>
#include <vector>

// Forward declare only what we need from txnaddamount to avoid including its header,
// which currently has incompatible overrides with Command (compilation failure).
namespace org { namespace minima { namespace system { namespace commands { namespace txn {
class txnaddamount {
public:
    static void enableCoinLock(bool zCoinLockEnabled);
    static bool isCoinLockEnabled();
};
} } } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace txn {

txncoinlock::txncoinlock()
    : org::minima::system::commands::Command(
          "txncoinlock",
          "(action:) - Lock coins used in un-broadcast transactions") {}

txncoinlock::~txncoinlock() = default;

std::string txncoinlock::getFullHelp() const {
    return "\txncoinlock\n"
           "\n"
           "Lock coins used in un-broadcast transactions.\n"
           "\n"
           "\n"
           "action: (optional)\n"
           "    lock - lock coins.\n"
           "    unlock - unlock coins.\n"
           "\n"
           "Examples:\n"
           "\n"
           "txncoinlock\n"
           "\n"
           "txncoinlock action:lock\n"
           "\n"
           "txncoinlock action:unlock\n"
           "\n";
}

std::vector<std::string> txncoinlock::getValidParams() const {
    return std::vector<std::string>{ "action" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> txncoinlock::runCommand() {
    using org::minima::utils::json::JSONObject;

    // Base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Lock or unlock
    std::string action = getParam("action", "");

    if (action == "lock") {
        org::minima::system::commands::txn::txnaddamount::enableCoinLock(true);
    } else if (action == "unlock") {
        org::minima::system::commands::txn::txnaddamount::enableCoinLock(false);
    }

    JSONObject resp;
    resp.put("coinslocked",
             org::minima::system::commands::txn::txnaddamount::isCoinLockEnabled());

    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* txncoinlock::getFunction() {
    return new txncoinlock();
}

} // namespace txn
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org