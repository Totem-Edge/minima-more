#pragma once

#include <string>
#include <vector>
#include <ctime>

#include "org/minima/utils/json/j_s_o_n_array.hpp"

namespace org {
namespace minima {
namespace utils {

class MiniUtil {
public:
    // Emulates Java's public static final SimpleDateFormat DATEFORMAT
    class DateFormat {
    public:
        DateFormat() = default;
        ~DateFormat() = default;

        // Returns the Java pattern string
        std::string pattern() const;

        // Format a std::time_t using the pattern dd_MM_yyyy_HHmmss in local time
        std::string format(std::time_t t) const;

    private:
        static std::tm safeLocalTime(std::time_t t);
    };

    // Accessor for the static DateFormat instance (equivalent to Java's static final field)
    static const DateFormat& DATEFORMAT();

    // Convert a list of strings to a JSONArray of strings
    static org::minima::utils::json::JSONArray convertArrayList(const std::vector<std::string>& zStringList);

    // Convert a JSONArray of strings to a vector<string>
    // Throws std::bad_any_cast if any element is not a std::string (mirrors Java ClassCastException behavior)
    static std::vector<std::string> convertJSONArray(const org::minima::utils::json::JSONArray& zStringArray);
};

} // namespace utils
} // namespace minima
} // namespace org