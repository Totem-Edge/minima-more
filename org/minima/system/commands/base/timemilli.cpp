#include "org/minima/system/commands/base/timemilli.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/objects/base/mini_number.hpp"

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace base {

timemilli::timemilli()
    : org::minima::system::commands::Command("timemilli", "Returns the current time in milliseconds") {
}

std::string timemilli::getFullHelp() const {
    return "\ttimemilli\n"
           "\n"
           "Return the current timemilli. Can go back minutes or hours.\n"
           "\n"
           "minutesback: (optional)\n"
           "    Go back this many minutes\n"
           "\n"
           "hoursback: (optional)\n"
           "    Go back this many hours\n"
           "\n"
           "Examples:\n"
           "\n"
           "timemilli\n"
           "\n"
           "timemilli minutesback:120\n"
           "\n"
           "timemilli hoursback:24\n"
           "\n";
}

std::vector<std::string> timemilli::getValidParams() const {
    return {"minutesback", "hoursback"};
}

static std::string formatLocalDateFromMillis(std::int64_t millis) {
    std::time_t tt = static_cast<std::time_t>(millis / 1000);
    std::tm tmval{};
#ifdef _WIN32
    localtime_s(&tmval, &tt);
#else
    localtime_r(&tt, &tmval);
#endif
    std::ostringstream oss;
    // Approximate Java Date.toString() format: "EEE MMM dd HH:mm:ss zzz yyyy"
    oss << std::put_time(&tmval, "%a %b %d %H:%M:%S %Z %Y");
    return oss.str();
}

std::unique_ptr<org::minima::utils::json::JSONObject> timemilli::runCommand() {
    using org::minima::utils::json::JSONObject;

    auto ret = getJSONReply();

    // Current time in milliseconds
    auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    std::int64_t timenow = now.time_since_epoch().count();

    if (existsParam("minutesback")) {
        // minutesback in minutes
        std::unique_ptr<org::minima::objects::base::MiniNumber> mins = getNumberParam("minutesback");
        // 1000 ms * 60 s/min * minutes
        std::int64_t minback = 1000LL * 60LL * static_cast<std::int64_t>(mins->getAsInt());
        timenow -= minback;
    } else if (existsParam("hoursback")) {
        std::unique_ptr<org::minima::objects::base::MiniNumber> hours = getNumberParam("hoursback");
        // 1000 ms * 60 s/min * 60 min/hour * hours
        std::int64_t minback = 1000LL * 60LL * 60LL * static_cast<std::int64_t>(hours->getAsInt());
        timenow -= minback;
    }

    // Format date string similar to Java Date.toString()
    std::string dateStr = formatLocalDateFromMillis(timenow);

    JSONObject resp;
    resp.put("timemilli", std::any(timenow));
    resp.put("date", std::any(dateStr));

    ret->put("response", std::any(resp));

    return ret;
}

org::minima::system::commands::Command* timemilli::getFunction() {
    return new timemilli();
}

} // namespace base
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org