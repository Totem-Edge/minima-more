#pragma once

// File: org/minima/utils/json\j_s_o_n_stream_aware.hpp

namespace org {
namespace minima {
namespace utils {
namespace json {

class JSONWriter;

/**
 * Beans that support customized output of JSON text to a writer shall implement this interface.
 * C++ equivalent of the Java interface org.minima.utils.json.JSONStreamAware.
 */
class JSONStreamAware {
public:
    virtual ~JSONStreamAware() noexcept;

    /**
     * Write JSON string to the provided writer.
     * Implementations may throw exceptions (e.g., std::ios_base::failure) on I/O errors.
     */
    virtual void writeJSONString(JSONWriter& out) const = 0;
};

} // namespace json
} // namespace utils
} // namespace minima
} // namespace org