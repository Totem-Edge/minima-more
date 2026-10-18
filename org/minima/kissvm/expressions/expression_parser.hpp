#pragma once

#include <memory>
#include <vector>
#include <string>

// Forward declarations for project types used in signatures
namespace org { namespace minima { namespace kissvm { namespace tokens {
class LexicalTokenizer;
class ScriptToken;
} } } }

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

class Expression;

class ExpressionParser {
public:
    // Entry point: build an Expression from a flat list of tokens with an initial stack depth
    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getExpression(const std::vector<org::minima::kissvm::tokens::ScriptToken>& zTokens, int zStackDepth);

    // Recursive-descent root using a LexicalTokenizer
    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getExpression(org::minima::kissvm::tokens::LexicalTokenizer& zTokens);

private:
    // Helper recursive-descent levels (operator precedence hierarchy)
    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getRelation(org::minima::kissvm::tokens::LexicalTokenizer& zTokens);

    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getLogic(org::minima::kissvm::tokens::LexicalTokenizer& zTokens);

    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getAddSub(org::minima::kissvm::tokens::LexicalTokenizer& zTokens);

    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getMulDiv(org::minima::kissvm::tokens::LexicalTokenizer& zTokens);

    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getPrimary(org::minima::kissvm::tokens::LexicalTokenizer& zTokens);

    static std::unique_ptr<org::minima::kissvm::expressions::Expression>
    getBaseUnit(org::minima::kissvm::tokens::LexicalTokenizer& zTokens);
};

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org