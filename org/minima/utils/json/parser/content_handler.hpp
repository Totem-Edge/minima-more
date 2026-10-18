#pragma once

#include <string>
#include <any>

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

/**
 * A simplified and stoppable SAX-like content handler for stream processing of JSON text.
 *
 * Semantics mirror the original Java interface:
 * - Methods may throw exceptions to signal errors; returning false from boolean
 *   methods signals that parsing should stop after the return.
 * - For primitive(value), pass an empty std::any to represent JSON null.
 */
class ContentHandler {
public:
    virtual ~ContentHandler();

    // Receive notification of the beginning of JSON processing.
    // Called only once by the parser.
    virtual void startJSON() = 0;

    // Receive notification of the end of JSON processing.
    virtual void endJSON() = 0;

    // Receive notification of the beginning of a JSON object.
    // Return false to request the parser to stop after this return.
    virtual bool startObject() = 0;

    // Receive notification of the end of a JSON object.
    // Return false to request the parser to stop after this return.
    virtual bool endObject() = 0;

    // Receive notification of the beginning of a JSON object entry with the given key.
    // Return false to request the parser to stop after this return.
    virtual bool startObjectEntry(const std::string& key) = 0;

    // Receive notification of the end of the value of the previous object entry.
    // Return false to request the parser to stop after this return.
    virtual bool endObjectEntry() = 0;

    // Receive notification of the beginning of a JSON array.
    // Return false to request the parser to stop after this return.
    virtual bool startArray() = 0;

    // Receive notification of the end of a JSON array.
    // Return false to request the parser to stop after this return.
    virtual bool endArray() = 0;

    // Receive notification of JSON primitive values:
    //   std::string, numeric types (e.g., double/int64 via std::any), bool, or null.
    // Represent JSON null by passing an empty std::any (i.e., value.has_value() == false).
    // Return false to request the parser to stop after this return.
    virtual bool primitive(const std::any& value) = 0;
};

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org