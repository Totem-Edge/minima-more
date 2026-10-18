#pragma once

#include <any>
#include <cstdint>
#include <string>
#include <vector>

#include "org/minima/utils/json/j_s_o_n_value.hpp"
#include "org/minima/utils/json/j_s_o_n_writer.hpp"
#include "org/minima/utils/json/j_s_o_n_aware.hpp"
#include "org/minima/utils/json/j_s_o_n_stream_aware.hpp"

namespace org {
namespace minima {
namespace utils {
namespace json {

// Minimal interfaces mirroring Java JSONAware and JSONStreamAware contracts.
// class JSONAware {
// public:
//     virtual ~JSONAware() = default;
//     virtual std::string toJSONString() const = 0;
// };

// class JSONStreamAware {
// public:
//     virtual ~JSONStreamAware() = default;
//     virtual void writeJSONString(JSONWriter& out) const = 0;
// };

class JSONArray : public JSONAware, public JSONStreamAware {
public:
    // Constructors
    JSONArray();
    explicit JSONArray(const std::vector<std::any>& collection);

    // Mutators and accessors to mimic a simple list behavior
    void add(const std::any& value);
    std::size_t size() const;
    const std::any& at(std::size_t idx) const;
    std::any& at(std::size_t idx);
    const std::vector<std::any>& elements() const;

    void writeJSONString(JSONWriter& out) const override;
    std::string toJSONString() const override;

    // Equivalent to Java toString(): returns toJSONString()
    std::string toString() const;
    // std::string toJSONString() const; 

    // Static encoders for collections (JSONArray)
    static void writeJSONString(const JSONArray* collection, JSONWriter& out);
    static void writeJSONString(const JSONArray& collection, JSONWriter& out);

    static std::string toJSONString(const JSONArray* collection);
    static std::string toJSONString(const JSONArray& collection);

    // Static encoders for primitive arrays

    // byte[] -> int8_t
    static void writeJSONString(const std::vector<int8_t>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<int8_t>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<int8_t>* array);
    static std::string toJSONString(const std::vector<int8_t>& array);

    // short[] -> int16_t
    static void writeJSONString(const std::vector<int16_t>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<int16_t>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<int16_t>* array);
    static std::string toJSONString(const std::vector<int16_t>& array);

    // int[] -> int32_t
    static void writeJSONString(const std::vector<int32_t>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<int32_t>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<int32_t>* array);
    static std::string toJSONString(const std::vector<int32_t>& array);

    // long[] -> int64_t
    static void writeJSONString(const std::vector<int64_t>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<int64_t>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<int64_t>* array);
    static std::string toJSONString(const std::vector<int64_t>& array);

    // float[]
    static void writeJSONString(const std::vector<float>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<float>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<float>* array);
    static std::string toJSONString(const std::vector<float>& array);

    // double[]
    static void writeJSONString(const std::vector<double>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<double>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<double>* array);
    static std::string toJSONString(const std::vector<double>& array);

    // boolean[] -> bool
    static void writeJSONString(const std::vector<bool>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<bool>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<bool>* array);
    static std::string toJSONString(const std::vector<bool>& array);

    // char[]
    static void writeJSONString(const std::vector<char>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<char>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<char>* array);
    static std::string toJSONString(const std::vector<char>& array);

    // Object[] -> std::any
    static void writeJSONString(const std::vector<std::any>* array, JSONWriter& out);
    static void writeJSONString(const std::vector<std::any>& array, JSONWriter& out);
    static std::string toJSONString(const std::vector<std::any>* array);
    static std::string toJSONString(const std::vector<std::any>& array);

private:
    std::vector<std::any> m_elements;
};

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org