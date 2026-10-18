#include "org/minima/utils/json/j_s_o_n_object.hpp"

#include <utility>
#include <string>

namespace org {
namespace minima {
namespace utils {
namespace json {

JSONObject::JSONObject() = default;

JSONObject::JSONObject(const std::vector<std::pair<std::string, std::any>>& entries) {
    m_entries.reserve(entries.size());
    for (const auto& kv : entries) {
        put(kv.first, kv.second);
    }
}

void JSONObject::put(const std::string& key, const std::any& value) {
    auto it = m_index.find(key);
    if (it != m_index.end()) {
        // Update existing without changing insertion order
        m_entries[it->second].second = value;
    } else {
        std::size_t idx = m_entries.size();
        m_entries.emplace_back(key, value);
        m_index.emplace(key, idx);
    }
}

bool JSONObject::containsKey(const std::string& key) const {
    return m_index.find(key) != m_index.end();
}

const std::any& JSONObject::get(const std::string& key) const {
    auto it = m_index.find(key);
    if (it == m_index.end()) {
        throw std::out_of_range("JSONObject: key not found: " + key);
    }
    return m_entries[it->second].second;
}

std::size_t JSONObject::size() const {
    return m_entries.size();
}

void JSONObject::writeJSONString(const JSONObject* map, JSONWriter& out) {
    if (map == nullptr) {
        out.write(std::string("null"));
        return;
    }

    bool first = true;
    out.write(std::string("{"));
    for (const auto& entry : map->m_entries) {
        if (first) {
            first = false;
        } else {
            out.write(std::string(","));
        }

        // Key
        out.write(std::string("\""));
        out.write(JSONObject::escape(entry.first));
        out.write(std::string("\""));

        out.write(std::string(":"));

        // Value
        JSONValue::writeJSONString(entry.second, out);
    }
    out.write(std::string("}"));
}

void JSONObject::writeJSONString(JSONWriter& out) const {
    JSONObject::writeJSONString(this, out);
}

std::string JSONObject::toJSONString(const JSONObject* map) {
    JSONWriter writer;
    try {
        writeJSONString(map, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        // Match Java's RuntimeException semantics on unexpected I/O issues.
        throw std::runtime_error(std::string("JSONObject::toJSONString failed: ") + e.what());
    }
}

std::string JSONObject::toJSONString() const {
    return toJSONString(this);
}

std::string JSONObject::toString() const {
    return toJSONString();
}

std::string JSONObject::getString(const std::string& zKey) const {
    const std::any& v = get(zKey);
    try {
        return std::any_cast<std::string>(v);
    } catch (const std::bad_any_cast&) {
        throw std::runtime_error("JSONObject: value for key '" + zKey + "' is not a std::string");
    }
}

bool JSONObject::getBoolean(const std::string& zKey) const {
    const std::any& v = get(zKey);
    try {
        return std::any_cast<bool>(v);
    } catch (const std::bad_any_cast&) {
        throw std::runtime_error("JSONObject: value for key '" + zKey + "' is not a bool");
    }
}

std::string JSONObject::getString(const std::string& zKey, const std::string& zDefault) const {
    if (containsKey(zKey)) {
        return getString(zKey);
    }
    return zDefault;
}

std::string JSONObject::toString(const std::string& key, const std::any& value) {
    std::string s;
    s.reserve(32); // small reserve
    s.push_back('\"');
    // In this overload, key is non-null; escape normally.
    s.append(JSONValue::escape(key));
    s.push_back('\"');
    s.push_back(':');
    s.append(JSONValue::toJSONString(value));
    return s;
}

std::string JSONObject::toString(const char* key, const std::any& value) {
    std::string s;
    s.reserve(32);
    s.push_back('\"');
    if (key == nullptr) {
        s.append("null");
    } else {
        s.append(JSONValue::escape(std::string(key)));
    }
    s.push_back('\"');
    s.push_back(':');
    s.append(JSONValue::toJSONString(value));
    return s;
}

std::string JSONObject::escape(const std::string& s) {
    return JSONValue::escape(s);
}

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org