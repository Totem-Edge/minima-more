#pragma once

#include <exception>
#include <string>
#include <any>

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

class ParseException : public std::exception {
public:
    // Error type constants
    static constexpr int ERROR_UNEXPECTED_CHAR = 0;
    static constexpr int ERROR_UNEXPECTED_TOKEN = 1;
    static constexpr int ERROR_UNEXPECTED_EXCEPTION = 2;

    // Constructors mirroring Java
    explicit ParseException(int errorType);
    ParseException(int errorType, const std::any& unexpectedObject);
    ParseException(int position, int errorType, const std::any& unexpectedObject);

    // Accessors/mutators
    int getErrorType() const;
    void setErrorType(int errorType);

    int getPosition() const;
    void setPosition(int position);

    std::any getUnexpectedObject() const;
    void setUnexpectedObject(const std::any& unexpectedObject);

    // Message (Java-style)
    std::string getMessage() const;

    // std::exception interface
    const char* what() const noexcept override;

private:
    int m_errorType;
    std::any m_unexpectedObject;
    int m_position;

    // Cache for what()
    mutable std::string m_cachedMessage;

    // Helpers
    std::string buildMessage() const;
    static std::string anyToString(const std::any& value);
};

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org