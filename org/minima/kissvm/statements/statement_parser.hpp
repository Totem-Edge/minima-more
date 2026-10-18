#pragma once

#include <memory>
#include <vector>
#include <string>

// Forward declarations for project types used in signatures (Rule 10)
namespace org { namespace minima { namespace kissvm { class Contract; } } }
namespace org { namespace minima { namespace kissvm { namespace expressions { class Expression; } } } }
namespace org { namespace minima { namespace kissvm { namespace statements { class StatementBlock; class Statement; } } } }
namespace org { namespace minima { namespace kissvm { namespace tokens { class ScriptToken; class LexicalTokenizer; } } } }

namespace org {
namespace minima {
namespace kissvm {
namespace statements {

class StatementParser {
public:
    // Parse a list of tokens into a StatementBlock at the given stack depth.
    // May throw org::minima::kissvm::exceptions::MinimaParseException or other exceptions from expression parsing.
    static std::unique_ptr<org::minima::kissvm::statements::StatementBlock>
    parseTokens(const std::vector<org::minima::kissvm::tokens::ScriptToken>& zTokens, int zStackDepth);

private:
    // Returns tokens from current position until the next ENDIF or (if zElseAlso) ELSEIF/ELSE/ENDIF at the current nesting level.
    static std::vector<org::minima::kissvm::tokens::ScriptToken>
    getElseOrElseIfOrEndIF(const std::vector<org::minima::kissvm::tokens::ScriptToken>& zTokens,
                           int zCurrentPosition,
                           bool zElseAlso);

    // Returns tokens from current position until the matching ENDWHILE at the current nesting level.
    static std::vector<org::minima::kissvm::tokens::ScriptToken>
    getEndWHILE(const std::vector<org::minima::kissvm::tokens::ScriptToken>& zTokens,
                int zCurrentPosition);

    // Returns tokens from current position up to (but not including) the very next COMMAND token.
    static std::vector<org::minima::kissvm::tokens::ScriptToken>
    getTokensToNextCommand(const std::vector<org::minima::kissvm::tokens::ScriptToken>& zTokens,
                           int zCurrentPosition);

    // Returns tokens from current position up to (but not including) the first occurrence of zRequiredToken.
    // Throws MinimaParseException if zRequiredToken is not found.
    static std::vector<org::minima::kissvm::tokens::ScriptToken>
    getTokensToRequiredCommand(const std::vector<org::minima::kissvm::tokens::ScriptToken>& zTokens,
                               int zCurrentPosition,
                               const std::string& zRequiredToken);

    // Returns tokens from current position up to (but not including) the next '=' operator token.
    static std::vector<org::minima::kissvm::tokens::ScriptToken>
    getTokensToNextEquals(const std::vector<org::minima::kissvm::tokens::ScriptToken>& zTokens,
                          int zCurrentPosition);
};

} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org