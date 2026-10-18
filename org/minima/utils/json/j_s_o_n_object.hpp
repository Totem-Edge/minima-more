#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <any>
#include <stdexcept>
#include <cstddef>

#include "org/minima/utils/json/j_s_o_n_value.hpp"
#include "org/minima/utils/json/j_s_o_n_writer.hpp"

namespace org {
namespace minima {
namespace utils {
namespace json {

class JSONObject {
public:
    // Constructors
    JSONObject();
    // Construct from a list of entries (insertion order preserved)
    explicit JSONObject(const std::vector<std::pair<std::string, std::any>>& entries);

    // Map-like operations
    void put(const std::string& key, const std::any& value);
    bool containsKey(const std::string& key) const;
    const std::any& get(const std::string& key) const;
    std::size_t size() const;

    // JSONStreamAware equivalent
    static void writeJSONString(const JSONObject* map, JSONWriter& out);
    void writeJSONString(JSONWriter& out) const;

    // JSONAware equivalents
    static std::string toJSONString(const JSONObject* map);
    std::string toJSONString() const;
    std::string toString() const;

    // Convenience getters
    std::string getString(const std::string& zKey) const;
    bool getBoolean(const std::string& zKey) const;
    std::string getString(const std::string& zKey, const std::string& zDefault) const;

    // Utilities
    static std::string toString(const std::string& key, const std::any& value);
    // Overload to reflect Java's null-key behavior
    static std::string toString(const char* key, const std::any& value);
    static std::string escape(const std::string& s);

private:
    // Preserve insertion order with vector, provide O(1) lookup with index map.
    std::vector<std::pair<std::string, std::any>> m_entries;
    std::unordered_map<std::string, std::size_t> m_index;
};

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org