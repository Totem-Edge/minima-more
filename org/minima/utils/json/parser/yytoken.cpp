#include "org/minima/utils/json/parser/yytoken.hpp"

#include <sstream>
#include <limits>

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

Yytoken::Yytoken(int t, Value v)
    : type(t), value(std::move(v)) {}

std::string Yytoken::toString() const {
    std::ostringstream sb;
    switch (type) {
        case TYPE_VALUE: {
            // Convert the variant value to string
            auto valueToString = [](const Value& v) -> std::string {
                return std::visit([](const auto& val) -> std::string {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        // No value payload; Java would append "null" only when actual value is null.
                        // For monostate, return empty to match "VALUE()" if misused, but this state
                        // should not occur for TYPE_VALUE in correct usage.
                        return "";
                    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                        return "null";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        return val ? "true" : "false";
                    } else if constexpr (std::is_same_v<T, long long>) {
                        return std::to_string(val);
                    } else if constexpr (std::is_same_v<T, double>) {
                        std::ostringstream oss;
                        oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
                        oss.precision(std::numeric_limits<double>::max_digits10);
                        oss << val;
                        return oss.str();
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        return val;
                    } else {
                        return std::string();
                    }
                }, v);
            };
            sb << "VALUE(" << valueToString(value) << ")";
            break;
        }
        case TYPE_LEFT_BRACE:
            sb << "LEFT BRACE({)";
            break;
        case TYPE_RIGHT_BRACE:
            sb << "RIGHT BRACE(})";
            break;
        case TYPE_LEFT_SQUARE:
            sb << "LEFT SQUARE([)";
            break;
        case TYPE_RIGHT_SQUARE:
            sb << "RIGHT SQUARE(])";
            break;
        case TYPE_COMMA:
            sb << "COMMA(,)";
            break;
        case TYPE_COLON:
            sb << "COLON(:)";
            break;
        case TYPE_EOF:
            sb << "END OF FILE";
            break;
        default:
            // Match Java behavior: no default text if type is unrecognized.
            break;
    }
    return sb.str();
}

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org