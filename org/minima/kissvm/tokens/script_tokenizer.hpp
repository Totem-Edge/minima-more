#pragma once

#include <string>
#include <vector>
#include <memory>

namespace org { namespace minima { namespace kissvm { namespace tokens { class ScriptToken; } } } }
namespace org { namespace minima { namespace kissvm { namespace exceptions { class MinimaParseException; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace tokens {

class ScriptTokenizer {
public:
    // Constructors
    explicit ScriptTokenizer(const std::string& zScript);
    ScriptTokenizer(const std::string& zScript, bool zCaseInsensitive);

    // Disable copy
    ScriptTokenizer(const ScriptTokenizer&) = delete;
    ScriptTokenizer& operator=(const ScriptTokenizer&) = delete;

    // Default move
    ScriptTokenizer(ScriptTokenizer&&) noexcept = default;
    ScriptTokenizer& operator=(ScriptTokenizer&&) noexcept = default;

    // Tokenize the input script
    // Throws org::minima::kissvm::exceptions::MinimaParseException on parse errors
    std::vector<ScriptToken> tokenize();
    static std::vector<ScriptToken> tokenize(const std::string& zScript);

    // Static utility checks (regex-based)
    static bool isNumeric(const std::string& zWord);
    static bool isHex(const std::string& zWord);
    static bool isVariable(const std::string& zWord);
    static bool isGlobal(const std::string& zWord);
    static bool isWhiteSpace(const std::string& zWord);
    static bool isValidFunctionName(const std::string& name);

private:
    // Retrieve the next word from the current position until whitespace or end-of-word symbols
    std::string getNextWord();

    // Static accessors for token tables
    static const std::vector<std::string>& tokensCommand();
    static const std::vector<std::string>& tokensNumberOperator();
    static const std::vector<std::string>& tokensBooleanOperator();
    static const std::vector<std::string>& tokensEndOfWord();
    static const std::vector<std::string>& tokensAfterBracket();

private:
    std::string mScript;
    int mPos;
    int mLength;
    bool mCaseInsensitive;
};

} // namespace tokens
} // namespace kissvm
} // namespace minima
} // namespace org