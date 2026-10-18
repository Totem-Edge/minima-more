#pragma once

#include <string>
#include <cstdint>

#include "org/minima/objects/base/mini_number.hpp"
#include "org/minima/utils/json/j_s_o_n_array.hpp"
#include "org/minima/utils/json/j_s_o_n_object.hpp"
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"
#include "org/minima/utils/json/parser/parse_exception.hpp"

namespace org {
namespace minima {
namespace utils {

class MiniFormat {
public:
    // Convert a JSON string into a JSONArray. Throws ParseException on parse errors.
    static org::minima::utils::json::JSONArray convertToJSON(const std::string& zJsonString);

    static std::string JSONPretty(const org::minima::utils::json::JSONArray& zJSONArray);
    static std::string JSONPretty(const org::minima::utils::json::JSONArray& zJSONArray, bool zDebug);

    static std::string JSONPretty(const org::minima::utils::json::JSONObject& zJSONObj);
    static std::string JSONPretty(const org::minima::utils::json::JSONObject& zJSONObj, bool zDebug);

    static std::string formatSize(long long v);

    static std::string zeroPad(int zTotLength, const org::minima::objects::base::MiniNumber& zNumber);

    static std::string ConvertMilliToTime(long long zMilli);

    static std::string createRandomString(int len);

private:
    static std::string maketabstring(int zNum);

    // Helpers
    static std::string trim(const std::string& s);
    static void replaceAllInPlace(std::string& s, const std::string& from, const std::string& to);
    static int numberOfLeadingZeros64(std::uint64_t x);

    // Any -> JSONObject and Any -> JSONArray conversions (by value)
    static org::minima::utils::json::JSONObject anyToJSONObject(const std::any& a);
    static org::minima::utils::json::JSONArray anyToJSONArray(const std::any& a);

    static std::string toBase32Upper(const std::vector<std::uint8_t>& data);
};

} // namespace utils
} // namespace minima
} // namespace org