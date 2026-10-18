#pragma once

#include <string>
#include <variant>

namespace org {
namespace minima {
namespace utils {
namespace json {
namespace parser {

class Yytoken {
public:
    // Token types
    static constexpr int TYPE_VALUE         = 0;   // JSON primitive value: string, number, boolean, null
    static constexpr int TYPE_LEFT_BRACE    = 1;
    static constexpr int TYPE_RIGHT_BRACE   = 2;
    static constexpr int TYPE_LEFT_SQUARE   = 3;
    static constexpr int TYPE_RIGHT_SQUARE  = 4;
    static constexpr int TYPE_COMMA         = 5;
    static constexpr int TYPE_COLON         = 6;
    static constexpr int TYPE_EOF           = -1;  // end of file

    // Value type:
    // - monostate: no value payload (used for structural tokens)
    // - nullptr_t: JSON null (used when type == TYPE_VALUE and the value is null)
    using Value = std::variant<std::monostate, std::nullptr_t, bool, long long, double, std::string>;

    // Public fields to mirror Java's public members
    int type = 0;
    Value value{};

    // Constructor equivalent to Java's Yytoken(int type, Object value)
    explicit Yytoken(int t, Value v = Value{});

    // toString equivalent
    std::string toString() const;
};

} // namespace parser
} // namespace json
} // namespace utils
} // namespace minima
} // namespace org