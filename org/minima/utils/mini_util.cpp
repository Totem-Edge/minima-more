#include "org/minima/utils/mini_util.hpp"

#include <sstream>
#include <iomanip>

namespace org {
namespace minima {
namespace utils {

// DateFormat

std::string MiniUtil::DateFormat::pattern() const {
    return "dd_MM_yyyy_HHmmss";
}

std::tm MiniUtil::DateFormat::safeLocalTime(std::time_t t) {
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    return tmv;
}

std::string MiniUtil::DateFormat::format(std::time_t t) const {
    // Map "dd_MM_yyyy_HHmmss" -> "%d_%m_%Y_%H%M%S"
    const char* fmt = "%d_%m_%Y_%H%M%S";
    std::tm tmv = safeLocalTime(t);
    std::ostringstream oss;
    oss << std::put_time(&tmv, fmt);
    return oss.str();
}

// Static DATEFORMAT accessor
const MiniUtil::DateFormat& MiniUtil::DATEFORMAT() {
    static const DateFormat s_instance;
    return s_instance;
}

// convertArrayList
org::minima::utils::json::JSONArray MiniUtil::convertArrayList(const std::vector<std::string>& zStringList) {
    org::minima::utils::json::JSONArray ret;
    for (const auto& str : zStringList) {
        ret.add(str);
    }
    return ret;
}

// convertJSONArray
std::vector<std::string> MiniUtil::convertJSONArray(const org::minima::utils::json::JSONArray& zStringArray) {
    std::vector<std::string> ret;
    const auto& elems = zStringArray.elements();
    ret.reserve(elems.size());
    for (const auto& obj : elems) {
        // Mirror Java's (String) cast behavior: throw if not a string
        ret.push_back(std::any_cast<std::string>(obj));
    }
    return ret;
}

} // namespace utils
} // namespace minima
} // namespace org