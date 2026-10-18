#include "org/minima/utils/json/j_s_o_n_array.hpp"

#include "org/minima/utils/json/j_s_o_n_aware.hpp"
#include "org/minima/utils/json/j_s_o_n_stream_aware.hpp"

#include <any>
#include <exception>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace org {
namespace minima {
namespace utils {
namespace json {

// Helper: detect null-like std::any
static inline bool is_any_null(const std::any& a) {
    if (!a.has_value()) return true;
    return a.type() == typeid(std::nullptr_t);
}

// Helper: format float/double similar to Java String.valueOf (close approximation)
template <typename FP>
static std::string fp_to_string(FP value) {
    std::ostringstream oss;
    oss << std::defaultfloat;
    oss.precision(std::numeric_limits<FP>::max_digits10);
    oss << value;
    return oss.str();
}

// JSONArray implementation

JSONArray::JSONArray() : m_elements() {}

JSONArray::JSONArray(const std::vector<std::any>& collection) : m_elements(collection) {}

void JSONArray::add(const std::any& value) {
    m_elements.push_back(value);
}

std::size_t JSONArray::size() const {
    return m_elements.size();
}

const std::any& JSONArray::at(std::size_t idx) const {
    return m_elements.at(idx);
}

std::any& JSONArray::at(std::size_t idx) {
    return m_elements.at(idx);
}

const std::vector<std::any>& JSONArray::elements() const {
    return m_elements;
}

void JSONArray::writeJSONString(JSONWriter& out) const {
    JSONArray::writeJSONString(*this, out);
}

std::string JSONArray::toJSONString() const {
    return JSONArray::toJSONString(*this);
}

std::string JSONArray::toString() const {
    return toJSONString();
}

// std::string JSONArray::toJSONString() const {
//     // Create a temporary writer and serialize
//     JSONWriter writer;
    
//     // Manually write the JSON (copy the logic from writeJSONString)
//     bool first = true;
//     writer.write('[');
//     for (const auto& val : m_elements) {
//         if (first) {
//             first = false;
//         } else {
//             writer.write(',');
//         }
//         if (is_any_null(val)) {
//             writer.write("null");
//         } else {
//             JSONValue::writeJSONString(val, writer);
//         }
//     }
//     writer.write(']');
    
//     return writer.toString();
// }
// Static encoders for collections

void JSONArray::writeJSONString(const JSONArray* collection, JSONWriter& out) {
    if (collection == nullptr) {
        out.write("null");
        return;
    }

    bool first = true;
    out.write('[');
    for (const auto& val : collection->m_elements) {
        if (first) {
            first = false;
        } else {
            out.write(',');
        }

        if (is_any_null(val)) {
            out.write("null");
            continue;
        }

        JSONValue::writeJSONString(val, out);
    }
    out.write(']');
}

void JSONArray::writeJSONString(const JSONArray& collection, JSONWriter& out) {
    JSONArray::writeJSONString(&collection, out);
}

std::string JSONArray::toJSONString(const JSONArray* collection) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(collection, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const JSONArray& collection) {
    return JSONArray::toJSONString(&collection);
}

// byte[] (int8_t)
void JSONArray::writeJSONString(const std::vector<int8_t>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        out.write(std::to_string(static_cast<int>((*array)[0])));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            out.write(std::to_string(static_cast<int>((*array)[i])));
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<int8_t>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<int8_t>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<int8_t>& array) {
    return JSONArray::toJSONString(&array);
}

// short[] (int16_t)
void JSONArray::writeJSONString(const std::vector<int16_t>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        out.write(std::to_string(static_cast<int>((*array)[0])));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            out.write(std::to_string(static_cast<int>((*array)[i])));
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<int16_t>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<int16_t>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<int16_t>& array) {
    return JSONArray::toJSONString(&array);
}

// int[] (int32_t)
void JSONArray::writeJSONString(const std::vector<int32_t>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        out.write(std::to_string((*array)[0]));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            out.write(std::to_string((*array)[i]));
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<int32_t>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<int32_t>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<int32_t>& array) {
    return JSONArray::toJSONString(&array);
}

// long[] (int64_t)
void JSONArray::writeJSONString(const std::vector<int64_t>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        out.write(std::to_string((*array)[0]));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            out.write(std::to_string((*array)[i]));
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<int64_t>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<int64_t>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<int64_t>& array) {
    return JSONArray::toJSONString(&array);
}

// float[]
void JSONArray::writeJSONString(const std::vector<float>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        out.write(fp_to_string((*array)[0]));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            out.write(fp_to_string((*array)[i]));
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<float>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<float>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<float>& array) {
    return JSONArray::toJSONString(&array);
}

// double[]
void JSONArray::writeJSONString(const std::vector<double>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        out.write(fp_to_string((*array)[0]));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            out.write(fp_to_string((*array)[i]));
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<double>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<double>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<double>& array) {
    return JSONArray::toJSONString(&array);
}

// boolean[] (bool)
void JSONArray::writeJSONString(const std::vector<bool>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        out.write((*array)[0] ? std::string("true") : std::string("false"));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            out.write((*array)[i] ? std::string("true") : std::string("false"));
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<bool>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<bool>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<bool>& array) {
    return JSONArray::toJSONString(&array);
}

// char[]
void JSONArray::writeJSONString(const std::vector<char>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write("[\"");
        out.write(std::string(1, (*array)[0]));
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write("\",\"");
            out.write(std::string(1, (*array)[i]));
        }
        out.write("\"]");
    }
}

void JSONArray::writeJSONString(const std::vector<char>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<char>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<char>& array) {
    return JSONArray::toJSONString(&array);
}

// Object[] (std::any)
void JSONArray::writeJSONString(const std::vector<std::any>* array, JSONWriter& out) {
    if (array == nullptr) {
        out.write("null");
    } else if (array->empty()) {
        out.write("[]");
    } else {
        out.write('[');
        if (is_any_null((*array)[0])) {
            out.write("null");
        } else {
            JSONValue::writeJSONString((*array)[0], out);
        }
        for (std::size_t i = 1; i < array->size(); ++i) {
            out.write(',');
            if (is_any_null((*array)[i])) {
                out.write("null");
            } else {
                JSONValue::writeJSONString((*array)[i], out);
            }
        }
        out.write(']');
    }
}

void JSONArray::writeJSONString(const std::vector<std::any>& array, JSONWriter& out) {
    JSONArray::writeJSONString(&array, out);
}

std::string JSONArray::toJSONString(const std::vector<std::any>* array) {
    try {
        JSONWriter writer;
        JSONArray::writeJSONString(array, writer);
        return writer.toString();
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
}

std::string JSONArray::toJSONString(const std::vector<std::any>& array) {
    return JSONArray::toJSONString(&array);
}

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org