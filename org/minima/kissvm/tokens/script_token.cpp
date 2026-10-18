#include "org/minima/kissvm/tokens/script_token.hpp"
#include <utility>
#include <vector>
#include <string>

namespace org {
namespace minima {
namespace kissvm {
namespace tokens {

// Forward declaration with member signature to avoid incomplete-type issues
class ScriptTokenizer {
public:
    static std::vector<ScriptToken> tokenize(const std::string& zScript);
};

ScriptToken::ScriptToken(int zTokenType, const std::string& zToken)
    : mTokenType(zTokenType), mToken(zToken) {}

int ScriptToken::getTokenType() const {
    return mTokenType;
}

std::string ScriptToken::getTokenTypeString() const {
    switch (mTokenType) {
        case TOKEN_CLOSEBRACKET:
            return "CLOSEBRACKET";
        case TOKEN_COMMAND:
            return "COMMAND";
        case TOKEN_TRUE:
            return "TRUE";
        case TOKEN_FALSE:
            return "FALSE";
        case TOKEN_FUNCTIION:
            return "FUNCTION";
        case TOKEN_FUNCTIIONPARAM:
            return "FUNCTIONPARAM";
        case TOKEN_VALUE:
            return "VALUE";
        case TOKEN_OPENBRACKET:
            return "OPENBRACKET";
        case TOKEN_OPERATOR:
            return "OPERATOR";
        case TOKEN_VARIABLE:
            return "VARIABLE";
        case TOKEN_GLOBAL:
            return "GLOBAL";
        default:
            break;
    }
    return "null";
}

std::string ScriptToken::getToken() const {
    return mToken;
}

std::string ScriptToken::toString() const {
    return getTokenTypeString() + ":" + getToken();
}

std::vector<ScriptToken> ScriptToken::tokenize(const std::string& zScript) {
    // Delegate to the tokenizer without requiring its full definition here.
    return ScriptTokenizer::tokenize(zScript);
}

} // namespace tokens
} // namespace kissvm
} // namespace minima
} // namespace org