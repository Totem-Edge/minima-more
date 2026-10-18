#include "org/minima/system/commands/base/maths.hpp"

#include <stdexcept>
#include <utility>

// Full headers for used types (Pitfall 12 / .cpp rule)
#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/objects/transaction.hpp"
#include "org/minima/objects/witness.hpp"
#include "org/minima/objects/state_variable.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

#ifdef _WIN32
// No Windows-specific code required here; placeholder for future platform nuances.
#endif

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

maths::maths()
    : org::minima::system::commands::Command(
          "maths",
          "[calculate:] (logs:)- Run maths with Minima precision MiniNumber") {}

std::string maths::getFullHelp() const {
    return "\nmaths\n"
           "\n"
           "Run some maths with Minima MiniNUmber precision\n"
           "\n"
           "Returns the result - na d full logs if required.\n"
           "\n"
           "calculate:\n"
           "    The maths you want calculated\n"
           "\n"
           "logs: (boolean)\n"
           "    true or false if you want the logs.\n"
           "\n"
           "Examples:\n"
           "\n"
           "maths calculate:\"1+2 * (3/4)\"\n"
           "\n"
           "maths calculate:\"1+2 * SIGDIG(1 3/4)\" logs:true\n";
}

std::vector<std::string> maths::getValidParams() const {
    return std::vector<std::string>{ "calculate", "logs" };
}

std::unique_ptr<org::minima::utils::json::JSONObject> maths::runCommand() {
    using org::minima::kissvm::Contract;
    using org::minima::objects::Transaction;
    using org::minima::objects::Witness;
    using org::minima::objects::StateVariable;
    using org::minima::objects::base::MiniData;
    using org::minima::utils::json::JSONObject;

    // Base JSON reply
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // Parameters
    std::string calculate = getParam("calculate");
    bool logs = getBooleanParam("logs", false);

    // Build script
    std::string function = std::string("LET returnvalue = ") + calculate;

    // Prepare empty signatures and prev-state
    std::vector<MiniData> sigs;
    std::vector<std::unique_ptr<StateVariable>> prev;

    // Create contract environment
    Witness wit;
    Transaction trx;

    Contract ctr(function, sigs, wit, trx, std::move(prev), false);

    // Execute
    ctr.run();

    // Fetch result
    const org::minima::kissvm::values::Value* val = ctr.getVariable("returnvalue");
    if (!val) {
        // Mirror Java NPE-like behavior if the variable is not set
        throw std::runtime_error("Contract did not set variable 'returnvalue'");
    }
    std::string res = val->toString();

    // Build response
    JSONObject resp;
    if (logs) {
        resp.put("logs", ctr.getCompleteTraceLog());
    }
    resp.put("result", res);

    // Attach and return
    ret->put("response", resp);
    return ret;
}

org::minima::system::commands::Command* maths::getFunction() {
    return new maths();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org