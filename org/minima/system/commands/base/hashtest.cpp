#include "org/minima/system/commands/base/hashtest.hpp"

#include <chrono>

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/minima_logger.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"

// Forward declare only what we need from TxPoWMiner to avoid including its header
namespace org { namespace minima { namespace objects { namespace base { class MiniNumber; } } } }
namespace org { namespace minima { namespace system { namespace brains {
class TxPoWMiner {
public:
    static org::minima::objects::base::MiniNumber
    calculateHashSpeed(const org::minima::objects::base::MiniNumber& zHashes);
};
} } } }

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

using org::minima::objects::base::MiniNumber;
using org::minima::utils::json::JSONObject;

hashtest::hashtest()
    : org::minima::system::commands::Command(
          "hashtest",
          "(amount:) - Check the speed of hashing of this device. Defaults to 1 million hashes") {
}

std::string hashtest::getFullHelp() const {
    return "\nhashtest\n"
           "\n"
           "Check the speed of hashing of this device. Defaults to 1 million hashes.\n"
           "\n"
           "Returns the time taken in milliseconds and speed in megahashes/second.\n"
           "\n"
           "E.g. A speed of 0.5 MH/s indicates 500000 hashes per second.\n"
           "\n"
           "amount: (optional)\n"
           "    Number of hashes to execute.\n"
           "\n"
           "Examples:\n"
           "\n"
           "hashtest\n"
           "\n"
           "hashtest amount:2000000\n";
}

std::vector<std::string> hashtest::getValidParams() const {
    return std::vector<std::string>{ "amount" };
}

std::unique_ptr<JSONObject> hashtest::runCommand() {
    // Base reply object
    std::unique_ptr<JSONObject> ret = getJSONReply();

    // How many hashes to perform (default 1,000,000)
    std::unique_ptr<MiniNumber> hashes = getNumberParam("amount", MiniNumber::MILLION());

    // Start timer (milliseconds since epoch)
    auto tstart = std::chrono::time_point_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now())
                      .time_since_epoch()
                      .count();

    // Calculate speed - FIX: receive by value, not as unique_ptr
    MiniNumber speed = org::minima::system::brains::TxPoWMiner::calculateHashSpeed(*hashes);

    // End timer and compute diff in ms
    auto tend = std::chrono::time_point_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now())
                    .time_since_epoch()
                    .count();
    long long timediff = static_cast<long long>(tend - tstart);

    // Log speed - FIX: call toString() on the value, not through a pointer
    org::minima::utils::MinimaLogger::log(std::string("Speed : ") + speed.toString());

    // Convert to Mega-hashes per second and limit to 4 significant digits
    MiniNumber megspeed = speed.div(MiniNumber::MILLION()).setSignificantDigits(4);

    // Build response
    JSONObject resp;
    // Store hashes as string representation for serialization
    resp.put("hashes", hashes->toString());
    resp.put("millitime", timediff);
    resp.put("speed", megspeed.toString() + " MH/s");

    // Add response
    ret->put("response", resp);

    return ret;
}

org::minima::system::commands::Command* hashtest::getFunction() {
    return new hashtest();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org