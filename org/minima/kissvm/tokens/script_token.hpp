#pragma once

#include <string>
#include <vector>
#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace tokens {

class ScriptToken {
public:
    // Public constants mirroring Java values
    static constexpr int TOKEN_COMMAND         = 0;
    static constexpr int TOKEN_FUNCTIION       = 1;  // Note the original spelling in Java
    static constexpr int TOKEN_OPERATOR        = 2;
    static constexpr int TOKEN_VALUE           = 3;
    static constexpr int TOKEN_VARIABLE        = 4;
    static constexpr int TOKEN_GLOBAL          = 6;
    static constexpr int TOKEN_OPENBRACKET     = 7;
    static constexpr int TOKEN_CLOSEBRACKET    = 8;
    static constexpr int TOKEN_TRUE            = 9;
    static constexpr int TOKEN_FALSE           = 10;
    static constexpr int TOKEN_FUNCTIIONPARAM  = 11;

    // Constructor
    ScriptToken(int zTokenType, const std::string& zToken);

    // Accessors
    int getTokenType() const;
    std::string getTokenTypeString() const;
    std::string getToken() const;

    // To-string
    std::string toString() const;

    // Utility function equivalent to Java's static tokenize
    static std::vector<ScriptToken> tokenize(const std::string& zScript);

private:
    int         mTokenType;
    std::string mToken;
};

} // namespace tokens
} // namespace kissvm
} // namespace minima
} // namespace org