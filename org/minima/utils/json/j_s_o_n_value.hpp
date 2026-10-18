#pragma once

#include <any>
#include <istream>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace utils {
namespace json {

// Forward declarations to avoid including headers that define a conflicting JSONValue.
class JSONWriter;
class JSONAware;
class JSONStreamAware;
class JSONArray;
class JSONObject;

namespace parser {
class JSONParser;
}

/**
 * C++ translation of org.minima.utils.json.JSONValue (Java).
 * Provides JSON parsing helpers and generic JSON encoding utilities.
 */
class JSONValue {
public:
    // Parse JSON text into a C++ dynamic value (std::any), swallowing exceptions (returns empty any on error).
    static std::any parse(std::istream& in);
    static std::any parse(const std::string& s);

    // Parse JSON with exceptions (mirrors Java's parseWithException)
    static std::any parseWithException(std::istream& in);           // may throw parser::ParseException, i/o failures
    static std::any parseWithException(const std::string& s);       // may throw parser::ParseException

    // Encode an object (std::any) into JSON text written to out.
    // Throws on writer errors.
    static void writeJSONString(const std::any& value, JSONWriter& out);

    // Convert an object (std::any) to JSON text (throws std::runtime_error on unexpected writer failure)
    static std::string toJSONString(const std::any& value);

    // Escape control characters per JSONValue.escape in Java
    static std::string escape(const std::string& s);

    // Escape into an existing string buffer (appends to sb)
    static void escape(const std::string& s, std::string& sb);

private:
    // Helpers
    static void writeQuotedEscaped(const std::string& s, JSONWriter& out);
    static std::string numberToString(double d);
    static std::string numberToString(float f);
    template<typename IntT>
    static std::string integerToString(IntT v);

    // Array/object writers for common STL types
    static void writeJSONArray(const std::vector<std::any>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<std::string>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<bool>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<char>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<unsigned char>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<short>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<unsigned short>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<int>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<unsigned int>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<long long>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<unsigned long long>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<float>& arr, JSONWriter& out);
    static void writeJSONArray(const std::vector<double>& arr, JSONWriter& out);

    // JSON object writer for std::map<string, any>
    static void writeJSONObject(const std::vector<std::pair<std::string, std::any>>& items, JSONWriter& out);

    // Fallback any->string for unknown types (best-effort)
    static std::string anyToStringFallback(const std::any& value);
};

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org