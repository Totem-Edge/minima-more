#include "org/minima/utils/json/j_s_o_n_value.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <typeindex>
#include <typeinfo>

// Avoid class JSONValue redefinition by temporarily renaming the token JSONValue
// while including headers that (incorrectly) define a minimal JSONValue class.
#define JSONValue JSONValue_minima_internal_tmp
#include "org/minima/utils/json/j_s_o_n_array.hpp"          // JSONWriter, JSONAware/JSONStreamAware, JSONArray
#undef JSONValue

// Include parser after applying the same rename to avoid accidental conflicts through its includes.
#define JSONValue JSONValue_minima_internal_tmp
#include "org/minima/utils/json/parser/j_s_o_n_parser.hpp"  // JSONParser
#undef JSONValue

namespace org {
namespace minima {
namespace utils {
namespace json {

using parser::JSONParser;

std::any JSONValue::parse(std::istream& in) {
    try {
        JSONParser parser;
        return parser.parse(in);
    } catch (...) {
        // Mirrors Java: swallow and return null (empty any)
        return std::any();
    }
}

std::any JSONValue::parse(const std::string& s) {
    try {
        JSONParser parser;
        return parser.parse(s);
    } catch (...) {
        // Mirrors Java: swallow and return null (empty any)
        return std::any();
    }
}

std::any JSONValue::parseWithException(std::istream& in) {
    JSONParser parser;
    return parser.parse(in); // may throw ParseException or i/o related exceptions
}

std::any JSONValue::parseWithException(const std::string& s) {
    JSONParser parser;
    return parser.parse(s); // may throw ParseException
}

void JSONValue::writeQuotedEscaped(const std::string& s, JSONWriter& out) {
    out.write('\"');
    std::string esc;
    esc.reserve(s.size() + 16);
    escape(s, esc);
    out.write(esc);
    out.write('\"');
}

std::string JSONValue::numberToString(double d) {
    std::ostringstream oss;
    // Use default formatting; precision set to produce round-trippable representation
    oss.precision(std::numeric_limits<double>::max_digits10);
    oss << d;
    return oss.str();
}

std::string JSONValue::numberToString(float f) {
    std::ostringstream oss;
    oss.precision(std::numeric_limits<float>::max_digits10);
    oss << f;
    return oss.str();
}

template<typename IntT>
std::string JSONValue::integerToString(IntT v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

static inline bool is_null_any(const std::any& a) {
    if (!a.has_value()) return true;
    return std::any_cast<const std::nullptr_t>(&a) != nullptr;
}

void JSONValue::writeJSONString(const std::any& value, JSONWriter& out) {
    // null / empty any
    if (is_null_any(value)) {
        out.write("null");
        return;
    }

    // C-string: put(key, "literal") decays to const char* and lands in std::any
    // as such. Must be quoted/escaped like a std::string, otherwise it falls
    // through to anyToStringFallback and is emitted raw -> invalid JSON
    // (e.g. "type":simple).
    if (const auto cs = std::any_cast<const char*>(&value)) {
        if (*cs == nullptr) out.write("null");
        else writeQuotedEscaped(std::string(*cs), out);
        return;
    }
    if (const auto cs = std::any_cast<char*>(&value)) {
        if (*cs == nullptr) out.write("null");
        else writeQuotedEscaped(std::string(*cs), out);
        return;
    }

    // String
    if (const auto s = std::any_cast<std::string>(&value)) {
        writeQuotedEscaped(*s, out);
        return;
    }

    // JSONObject
    if (const auto jobj = std::any_cast<JSONObject>(&value)) {
        jobj->writeJSONString(out);
        return;
    }
    
    // JSONArray
    if (const auto jarr = std::any_cast<JSONArray>(&value)) {
        jarr->writeJSONString(out);
        return;
    }

    // Double
    if (const auto d = std::any_cast<double>(&value)) {
        if (!std::isfinite(*d)) {
            out.write("null");
        } else {
            out.write(numberToString(*d));
        }
        return;
    }
    // Float
    if (const auto f = std::any_cast<float>(&value)) {
        if (!std::isfinite(*f)) {
            out.write("null");
        } else {
            out.write(numberToString(*f));
        }
        return;
    }

    // Integer-like numbers
    if (const auto v = std::any_cast<short>(&value)) {
        out.write(integerToString(*v));
        return;
    }
    if (const auto v = std::any_cast<unsigned short>(&value)) {
        out.write(integerToString(*v));
        return;
    }
    if (const auto v = std::any_cast<int>(&value)) {
        out.write(integerToString(*v));
        return;
    }
    if (const auto v = std::any_cast<unsigned int>(&value)) {
        out.write(integerToString(*v));
        return;
    }
    if (const auto v = std::any_cast<long>(&value)) {
        out.write(integerToString(*v));
        return;
    }
    if (const auto v = std::any_cast<unsigned long>(&value)) {
        out.write(integerToString(*v));
        return;
    }
    if (const auto v = std::any_cast<long long>(&value)) {
        out.write(integerToString(*v));
        return;
    }
    if (const auto v = std::any_cast<unsigned long long>(&value)) {
        out.write(integerToString(*v));
        return;
    }

    // Boolean
    if (const auto b = std::any_cast<bool>(&value)) {
        out.write(*b ? "true" : "false");
        return;
    }

    // JSONStreamAware (if provided as base pointer via std::shared_ptr)
    if (const auto sa = std::any_cast<std::shared_ptr<JSONStreamAware>>(&value)) {
        if (sa->get()) {
            (*sa)->writeJSONString(out);
        } else {
            out.write("null");
        }
        return;
    }

    // JSONAware (if provided as base pointer via std::shared_ptr)
    if (const auto aw = std::any_cast<std::shared_ptr<JSONAware>>(&value)) {
        if (aw->get()) {
            out.write((*aw)->toJSONString());
        } else {
            out.write("null");
        }
        return;
    }

    // Map-like (JSON object): std::map<std::string, std::any>
    if (const auto mp = std::any_cast<std::map<std::string, std::any>>(&value)) {
        // Transform map to vector of pairs to ensure stable iteration
        std::vector<std::pair<std::string, std::any>> items;
        items.reserve(mp->size());
        for (const auto& kv : *mp) items.emplace_back(kv.first, kv.second);
        writeJSONObject(items, out);
        return;
    }

    // Collections / arrays (support common C++ vector<> analogs)
    if (const auto arr = std::any_cast<std::vector<std::any>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<std::string>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<bool>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<char>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<unsigned char>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<short>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<unsigned short>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<int>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<unsigned int>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<long long>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<unsigned long long>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<float>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }
    if (const auto arr = std::any_cast<std::vector<double>>(&value)) {
        writeJSONArray(*arr, out);
        return;
    }

    // Fallback: approximate Java's value.toString()
    out.write(anyToStringFallback(value));
}

std::string JSONValue::toJSONString(const std::any& value) {
    JSONWriter writer;
    try {
        writeJSONString(value, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        // Should never happen for a string-backed writer; mirror Java RuntimeException
        throw std::runtime_error(e.what());
    } catch (...) {
        throw std::runtime_error("Unknown error in JSONValue::toJSONString");
    }
}

std::string JSONValue::escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    escape(s, out);
    return out;
}

void JSONValue::escape(const std::string& s, std::string& sb) {
    const size_t len = s.size();
    sb.reserve(sb.size() + len + 16);
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        switch (ch) {
        case '\"':
            sb.append("\\\"");
            break;
        case '\\':
            sb.append("\\\\");
            break;
        case '\b':
            sb.append("\\b");
            break;
        case '\f':
            sb.append("\\f");
            break;
        case '\n':
            sb.append("\\n");
            break;
        case '\r':
            sb.append("\\r");
            break;
        case '\t':
            sb.append("\\t");
            break;
        case '/':
            sb.append("\\/");
            break;
        default:
            // Control ranges: U+0000–U+001F, U+007F–U+009F
            if ((ch <= 0x1F) || (ch >= 0x7F && ch <= 0x9F)) {
                sb.append("\\u00");
                static const char* hex = "0123456789ABCDEF";
                sb.push_back(hex[(ch >> 4) & 0xF]);
                sb.push_back(hex[ch & 0xF]);
            } else {
                // Pass-through UTF-8 bytes for non-ASCII; JSON permits UTF-8
                sb.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
}

// JSONArray writers for vectors (emit [a,b,c])
void JSONValue::writeJSONArray(const std::vector<std::any>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        writeJSONString(arr[i], out);
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<std::string>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        writeQuotedEscaped(arr[i], out);
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<bool>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(arr[i] ? "true" : "false");
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<char>& arr, JSONWriter& out) {
    // Represent each char as a JSON string of length 1
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        std::string s(1, arr[i]);
        writeQuotedEscaped(s, out);
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<unsigned char>& arr, JSONWriter& out) {
    // Treat as numeric bytes
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(integerToString(static_cast<unsigned int>(arr[i])));
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<short>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(integerToString(arr[i]));
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<unsigned short>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(integerToString(arr[i]));
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<int>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(integerToString(arr[i]));
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<unsigned int>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(integerToString(arr[i]));
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<long long>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(integerToString(arr[i]));
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<unsigned long long>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        out.write(integerToString(arr[i]));
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<float>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        if (!std::isfinite(arr[i])) {
            out.write("null");
        } else {
            out.write(numberToString(arr[i]));
        }
    }
    out.write(']');
}

void JSONValue::writeJSONArray(const std::vector<double>& arr, JSONWriter& out) {
    out.write('[');
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i) out.write(',');
        if (!std::isfinite(arr[i])) {
            out.write("null");
        } else {
            out.write(numberToString(arr[i]));
        }
    }
    out.write(']');
}

void JSONValue::writeJSONObject(const std::vector<std::pair<std::string, std::any>>& items, JSONWriter& out) {
    out.write('{');
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out.write(',');
        // key
        writeQuotedEscaped(items[i].first, out);
        out.write(':');
        // value
        writeJSONString(items[i].second, out);
    }
    out.write('}');
}

std::string JSONValue::anyToStringFallback(const std::any& value) {
    // Best-effort approximations for some common cases not matched above
    if (const auto s = std::any_cast<const char*>(&value)) {
        return std::string(*s ? *s : "");
    }
    // As a last resort, use the type name
    std::ostringstream oss;
    oss << "<" << value.type().name() << ">";
    return oss.str();
}

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org