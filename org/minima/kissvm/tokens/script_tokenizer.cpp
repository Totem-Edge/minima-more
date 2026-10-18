#include "org/minima/kissvm/tokens/script_tokenizer.hpp"

#include <regex>
#include <stdexcept>
#include <algorithm> // for std::find
#include <cctype>    // for std::toupper, std::tolower

// Project headers (full includes in .cpp)
#include "org/minima/kissvm/tokens/script_token.hpp"
#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"
#include "org/minima/kissvm/functions/minima_function.hpp"

#ifdef _WIN32
// No OS-specific logic currently required; placeholder for future needs.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace tokens {

// Static token tables via accessor functions (thread-safe local statics)
const std::vector<std::string>& ScriptTokenizer::tokensCommand() {
    static const std::vector<std::string> cmds = {
        "LET",
        "IF","THEN","ELSEIF","ELSE","ENDIF",
        "RETURN",
        "ASSERT",
        "WHILE","DO","ENDWHILE",
        "EXEC",
        "MAST"
    };
    return cmds;
}

const std::vector<std::string>& ScriptTokenizer::tokensNumberOperator() {
    static const std::vector<std::string> ops = {
        "+","-","/","*","%","&","|","^","~","="
    };
    return ops;
}

const std::vector<std::string>& ScriptTokenizer::tokensBooleanOperator() {
    static const std::vector<std::string> bops = {
        "LT","LTE","GT","GTE","EQ","NEQ",
        "XOR","AND","OR",
        "NXOR","NAND","NOR",
        "NOT","NEG"
    };
    return bops;
}

const std::vector<std::string>& ScriptTokenizer::tokensEndOfWord() {
    static const std::vector<std::string> eow = {
        "+","-","/","*","%","&","|","^","~","=","(",")","[","]","<",">","<<",">>"
    };
    return eow;
}

const std::vector<std::string>& ScriptTokenizer::tokensAfterBracket() {
    static const std::vector<std::string> aft = {
        "+","-","/","*","%","&","|","^","=","<",">","<<",">>",")"
    };
    return aft;
}

// Constructors
ScriptTokenizer::ScriptTokenizer(const std::string& zScript)
    : ScriptTokenizer(zScript, false) {}

ScriptTokenizer::ScriptTokenizer(const std::string& zScript, bool zCaseInsensitive)
    : mScript(zScript),
      mPos(0),
      mLength(static_cast<int>(zScript.size())),
      mCaseInsensitive(zCaseInsensitive) {}

// Static regex-based utilities
bool ScriptTokenizer::isNumeric(const std::string& zWord) {
    static const std::regex re_numeric("^[0-9]+(\\.[0-9]+)?", std::regex::ECMAScript);
    return std::regex_match(zWord, re_numeric);
}

bool ScriptTokenizer::isHex(const std::string& zWord) {
    static const std::regex re_hex("^0x[0-9a-fA-F]+$", std::regex::ECMAScript);
    return std::regex_match(zWord, re_hex);
}

bool ScriptTokenizer::isVariable(const std::string& zWord) {
    static const std::regex re_var("^[a-z]+$", std::regex::ECMAScript);
    return std::regex_match(zWord, re_var);
}

bool ScriptTokenizer::isGlobal(const std::string& zWord) {
    static const std::regex re_global("^@[A-Z]+$", std::regex::ECMAScript);
    return std::regex_match(zWord, re_global);
}

bool ScriptTokenizer::isWhiteSpace(const std::string& zWord) {
    static const std::regex re_ws("^\\s+$", std::regex::ECMAScript);
    return std::regex_match(zWord, re_ws);
}

// getNextWord: accumulate characters until next EOW or whitespace, without consuming the EOW/WS
std::string ScriptTokenizer::getNextWord() {
    std::string word;
    const auto& eow = tokensEndOfWord();

    while (mPos < mLength) {
        char ch = mScript[static_cast<size_t>(mPos)];
        std::string c(1, ch);

        // End on EOW symbol or whitespace
        if (std::find(eow.begin(), eow.end(), c) != eow.end() || isWhiteSpace(c)) {
            break;
        }

        word.push_back(ch);
        ++mPos;
    }

    return word;
}

std::vector<ScriptToken> ScriptTokenizer::tokenize(const std::string& zScript) {
    ScriptTokenizer tokenizer(zScript);
    return tokenizer.tokenize();
}

// Helper function to check if a string is a valid function name
// This mimics the Java version's pre-built list approach
bool ScriptTokenizer::isValidFunctionName(const std::string& name) {
    try {
        auto fn = org::minima::kissvm::functions::MinimaFunction::getFunction(name);
        return (fn != nullptr);
    } catch (...) {
        // If getFunction throws an exception, it's not a valid function
        return false;
    }
}

// tokenize
std::vector<ScriptToken> ScriptTokenizer::tokenize() {
    using org::minima::kissvm::exceptions::MinimaParseException;
    std::vector<ScriptToken> tokens;

    const auto& allcommands = tokensCommand();
    const auto& allnumops   = tokensNumberOperator();
    const auto& allboolops  = tokensBooleanOperator();

    // Initialize
    mPos = 0;
    bool waslastspace = true;

    while (mPos < mLength) {
        std::string nextchar(1, mScript[static_cast<size_t>(mPos)]);

        bool wasspace = false;

        // Whitespace: skip and mark wasspace
        if (isWhiteSpace(nextchar)) {
            ++mPos;
            wasspace = true;

        // One-character number operators
        } else if (std::find(allnumops.begin(), allnumops.end(), nextchar) != allnumops.end()) {
            tokens.emplace_back(ScriptToken::TOKEN_OPERATOR, nextchar);
            ++mPos;

        // Check for <<
        } else if (nextchar == "<") {
            if (mPos + 1 >= mLength) {
                throw MinimaParseException("Incorrect Token found @ " + std::to_string(mPos) + " <");
            }
            std::string testchar(1, mScript[static_cast<size_t>(mPos + 1)]);
            if (testchar != "<") {
                throw MinimaParseException("Incorrect Token found @ " + std::to_string(mPos) + " " + nextchar + testchar);
            }
            tokens.emplace_back(ScriptToken::TOKEN_OPERATOR, std::string("<<"));
            mPos += 2;

        // Check for >>
        } else if (nextchar == ">") {
            if (mPos + 1 >= mLength) {
                throw MinimaParseException("Incorrect Token found @ " + std::to_string(mPos) + " >");
            }
            std::string testchar(1, mScript[static_cast<size_t>(mPos + 1)]);
            if (testchar != ">") {
                throw MinimaParseException("Incorrect Token found @ " + std::to_string(mPos) + " " + nextchar + testchar);
            }
            tokens.emplace_back(ScriptToken::TOKEN_OPERATOR, std::string(">>"));
            mPos += 2;

        // Round brackets
        } else if (nextchar == "(") {
            tokens.emplace_back(ScriptToken::TOKEN_OPENBRACKET, nextchar);
            ++mPos;

        } else if (nextchar == ")") {
            tokens.emplace_back(ScriptToken::TOKEN_CLOSEBRACKET, nextchar);
            ++mPos;

        // Square-bracketed string (with nesting)
        } else if (nextchar == "[") {
            std::string str;
            int sq = 0;

            while (mPos < mLength) {
                nextchar.assign(1, mScript[static_cast<size_t>(mPos)]);
                str += nextchar;

                if (nextchar == "[") {
                    ++sq;
                } else if (nextchar == "]") {
                    --sq;
                    if (sq == 0) {
                        ++mPos; // move past closing bracket
                        break;
                    }
                }
                ++mPos;
            }

            tokens.emplace_back(ScriptToken::TOKEN_VALUE, str);

        // Function parameter
        } else if (nextchar == "$") {
            std::string word = getNextWord();
            tokens.emplace_back(ScriptToken::TOKEN_FUNCTIIONPARAM, word);

        } else {
            // General word
            std::string word = getNextWord();

            // Prepare uppercase/lowercase variants if case-insensitive
            std::string uppercase = word;
            std::string lowercase = word;
            if (mCaseInsensitive) {
                uppercase.clear();
                lowercase.clear();
                uppercase.reserve(word.size());
                lowercase.reserve(word.size());
                for (char ch : word) {
                    unsigned char uch = static_cast<unsigned char>(ch);
                    uppercase.push_back(static_cast<char>(std::toupper(uch)));
                    lowercase.push_back(static_cast<char>(std::tolower(uch)));
                }
            }

            // Command
            const std::string& cmdcheck = mCaseInsensitive ? uppercase : word;
            if (std::find(allcommands.begin(), allcommands.end(), cmdcheck) != allcommands.end()) {
                if (!waslastspace) {
                    throw MinimaParseException("Missing space before Command @ " + std::to_string(mPos) + " " + cmdcheck);
                }
                tokens.emplace_back(ScriptToken::TOKEN_COMMAND, cmdcheck);

            // ══════════════════════════════════════════════════════════════
            // CRITICAL FIX: Check for numeric/hex values BEFORE functions!
            // This prevents hex literals like 0xABCD from being misclassified
            // as function names and fixes: "Parser Bug: Hex literal treated as function"
            // ══════════════════════════════════════════════════════════════
            } else if (isNumeric(word) || isHex(word)) {
                tokens.emplace_back(ScriptToken::TOKEN_VALUE, word);

            // ══════════════════════════════════════════════════════════════
            // CRITICAL FIX #2: Check if it's a function using safe method
            // Use isValidFunctionName() which catches exceptions instead of
            // letting MinimaFunction::getFunction() throw and crash the parser
            // This allows variables like 'h' to be correctly identified
            // ══════════════════════════════════════════════════════════════
            } else {
                const std::string& funcname = mCaseInsensitive ? uppercase : word;
                
                // Check if it's a valid function name (safely, without throwing)
                if (isValidFunctionName(funcname)) {
                    tokens.emplace_back(ScriptToken::TOKEN_FUNCTIION, funcname);

                // Boolean operator
                } else {
                    const std::string& bopcheck = mCaseInsensitive ? uppercase : word;
                    if (std::find(allboolops.begin(), allboolops.end(), bopcheck) != allboolops.end()) {
                        tokens.emplace_back(ScriptToken::TOKEN_OPERATOR, bopcheck);

                    // TRUE/FALSE
                    } else if ((mCaseInsensitive ? uppercase : word) == "TRUE") {
                        tokens.emplace_back(ScriptToken::TOKEN_TRUE, mCaseInsensitive ? uppercase : word);

                    } else if ((mCaseInsensitive ? uppercase : word) == "FALSE") {
                        tokens.emplace_back(ScriptToken::TOKEN_FALSE, mCaseInsensitive ? uppercase : word);

                    // Global
                    } else if (isGlobal(mCaseInsensitive ? uppercase : word)) {
                        tokens.emplace_back(ScriptToken::TOKEN_GLOBAL, mCaseInsensitive ? uppercase : word);

                    // Variable - this will now correctly match 'h', 'v', etc.
                    } else if (isVariable(mCaseInsensitive ? lowercase : word)) {
                        const std::string& varname = mCaseInsensitive ? lowercase : word;
                        if (varname.size() > 32) {
                            throw MinimaParseException("MAX Variable length is 32 @ " + std::to_string(mPos) + " " + word);
                        }
                        tokens.emplace_back(ScriptToken::TOKEN_VARIABLE, varname);

                    } else {
                        throw MinimaParseException("Incorrect Token found @ " + std::to_string(mPos) + " " + word);
                    }
                }
            }
        }

        // Save this - commands have to have this as true
        waslastspace = wasspace;
    }

    return tokens;
}

} // namespace tokens
} // namespace kissvm
} // namespace minima
} // namespace org