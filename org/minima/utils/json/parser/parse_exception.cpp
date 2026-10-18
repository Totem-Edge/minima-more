#include "org/minima/utils/json/parser/parse_exception.hpp"
#include "org/minima/utils/json/parser/yytoken.hpp"  // ADD THIS INCLUDE

#include <sstream>
#include <typeinfo>
#include <memory>

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

ParseException::ParseException(int errorType)
    : m_errorType(errorType), m_unexpectedObject(), m_position(-1), m_cachedMessage() {
}

ParseException::ParseException(int errorType, const std::any& unexpectedObject)
    : m_errorType(errorType), m_unexpectedObject(unexpectedObject), m_position(-1), m_cachedMessage() {
}

ParseException::ParseException(int position, int errorType, const std::any& unexpectedObject)
    : m_errorType(errorType), m_unexpectedObject(unexpectedObject), m_position(position), m_cachedMessage() {
}

int ParseException::getErrorType() const {
    return m_errorType;
}

void ParseException::setErrorType(int errorType) {
    m_errorType = errorType;
}

int ParseException::getPosition() const {
    return m_position;
}

void ParseException::setPosition(int position) {
    m_position = position;
}

std::any ParseException::getUnexpectedObject() const {
    return m_unexpectedObject;
}

void ParseException::setUnexpectedObject(const std::any& unexpectedObject) {
    m_unexpectedObject = unexpectedObject;
}

std::string ParseException::getMessage() const {
    // Build fresh each call to mirror Java semantics
    return buildMessage();
}

const char* ParseException::what() const noexcept {
    try {
        m_cachedMessage = buildMessage();
        return m_cachedMessage.c_str();
    } catch (...) {
        // In case of any error during message building, return a safe fallback
        return "ParseException";
    }
}

std::string ParseException::buildMessage() const {
    std::ostringstream sb;

    switch (m_errorType) {
        case ERROR_UNEXPECTED_CHAR:
            sb << "Unexpected character (" << anyToString(m_unexpectedObject)
               << ") at position " << m_position << ".";
            break;
        case ERROR_UNEXPECTED_TOKEN:
            sb << "Unexpected token " << anyToString(m_unexpectedObject)
               << " at position " << m_position << ".";
            break;
        case ERROR_UNEXPECTED_EXCEPTION:
            sb << "Unexpected exception at position " << m_position
               << ": " << anyToString(m_unexpectedObject);
            break;
        default:
            // Preserve original misspelling "Unkown"
            sb << "Unkown error at position " << m_position << ".";
            break;
    }

    return sb.str();
}

std::string ParseException::anyToString(const std::any& value) {
    if (!value.has_value()) {
        return "null";
    }

    // *** ADD THIS: Handle Yytoken objects ***
    if (value.type() == typeid(Yytoken)) {
        try {
            const Yytoken& token = std::any_cast<const Yytoken&>(value);
            std::ostringstream oss;
            
            // Format similar to Java's Yytoken.toString()
            switch (token.type) {
                case Yytoken::TYPE_VALUE:
                    // Display the value content
                    if (std::holds_alternative<std::string>(token.value)) {
                        oss << std::get<std::string>(token.value);
                    } else if (std::holds_alternative<long long>(token.value)) {
                        oss << std::get<long long>(token.value);
                    } else if (std::holds_alternative<double>(token.value)) {
                        oss << std::get<double>(token.value);
                    } else if (std::holds_alternative<bool>(token.value)) {
                        oss << (std::get<bool>(token.value) ? "true" : "false");
                    } else if (std::holds_alternative<std::nullptr_t>(token.value)) {
                        oss << "null";
                    } else {
                        oss << "[value]";
                    }
                    break;
                case Yytoken::TYPE_LEFT_BRACE:
                    oss << "{";
                    break;
                case Yytoken::TYPE_RIGHT_BRACE:
                    oss << "}";
                    break;
                case Yytoken::TYPE_LEFT_SQUARE:
                    oss << "[";
                    break;
                case Yytoken::TYPE_RIGHT_SQUARE:
                    oss << "]";
                    break;
                case Yytoken::TYPE_COMMA:
                    oss << ",";
                    break;
                case Yytoken::TYPE_COLON:
                    oss << ":";
                    break;
                case Yytoken::TYPE_EOF:
                    oss << "EOF";
                    break;
                default:
                    oss << "[unknown token type]";
                    break;
            }
            return oss.str();
        } catch (const std::bad_any_cast&) {
            // Fall through to other handlers
        }
    }

    // Helper lambdas to attempt safe any_casts and stringify common types.
    auto try_string = [&](const std::any& a) -> const std::string* {
        if (a.type() == typeid(std::string)) {
            return &std::any_cast<const std::string&>(a);
        }
        return nullptr;
    };

    // std::string
    if (auto s = try_string(value)) {
        return *s;
    }

    // const char*
    if (value.type() == typeid(const char*)) {
        const char* c = std::any_cast<const char*>(value);
        return c ? std::string(c) : std::string("null");
    }
    // char*
    if (value.type() == typeid(char*)) {
        const char* c = std::any_cast<char*>(value);
        return c ? std::string(c) : std::string("null");
    }

    // char
    if (value.type() == typeid(char)) {
        char c = std::any_cast<char>(value);
        return std::string(1, c);
    }
    // bool
    if (value.type() == typeid(bool)) {
        return std::any_cast<bool>(value) ? "true" : "false";
    }

    // Integer types
    if (value.type() == typeid(short)) {
        return std::to_string(std::any_cast<short>(value));
    }
    if (value.type() == typeid(unsigned short)) {
        return std::to_string(std::any_cast<unsigned short>(value));
    }
    if (value.type() == typeid(int)) {
        return std::to_string(std::any_cast<int>(value));
    }
    if (value.type() == typeid(unsigned int)) {
        return std::to_string(std::any_cast<unsigned int>(value));
    }
    if (value.type() == typeid(long)) {
        return std::to_string(std::any_cast<long>(value));
    }
    if (value.type() == typeid(unsigned long)) {
        return std::to_string(std::any_cast<unsigned long>(value));
    }
    if (value.type() == typeid(long long)) {
        return std::to_string(std::any_cast<long long>(value));
    }
    if (value.type() == typeid(unsigned long long)) {
        return std::to_string(std::any_cast<unsigned long long>(value));
    }

    // Floating point types
    if (value.type() == typeid(float)) {
        return std::to_string(std::any_cast<float>(value));
    }
    if (value.type() == typeid(double)) {
        return std::to_string(std::any_cast<double>(value));
    }
    if (value.type() == typeid(long double)) {
        // std::to_string has overload for long double in C++11+
        return std::to_string(static_cast<double>(std::any_cast<long double>(value)));
    }

    // std::exception_ptr: attempt to extract what()
    if (value.type() == typeid(std::exception_ptr)) {
        try {
            std::exception_ptr ep = std::any_cast<std::exception_ptr>(value);
            if (ep) {
                try {
                    std::rethrow_exception(ep);
                } catch (const std::exception& e) {
                    return std::string(e.what());
                } catch (...) {
                    return std::string("unknown exception");
                }
            } else {
                return "null";
            }
        } catch (...) {
            // fall through
        }
    }

    // Generic pointer types (void*, const void*)
    if (value.type() == typeid(void*)) {
        std::ostringstream oss;
        oss << std::any_cast<void*>(value);
        return oss.str();
    }
    if (value.type() == typeid(const void*)) {
        std::ostringstream oss;
        oss << std::any_cast<const void*>(value);
        return oss.str();
    }

    // Fallback: use type name as a placeholder representation.
    // This approximates Java's Object.toString() when no better info is available.
    return std::string(typeid(value).name());
}

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org