#include "org/minima/kissvm/expressions/expression_parser.hpp"

#include <utility>
#include <stdexcept>

// Tokens and tokenizer
#include "org/minima/kissvm/tokens/lexical_tokenizer.hpp"
#include "org/minima/kissvm/tokens/script_token.hpp"

// Exceptions
#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"

// Contract limits
#include "org/minima/kissvm/contract.hpp"

// Functions and values
#include "org/minima/kissvm/functions/minima_function.hpp"
#include "org/minima/kissvm/values/value.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"
#include "org/minima/kissvm/values/number_value.hpp"
#include "org/minima/objects/base/mini_number.hpp"

// Base Expression
#include "org/minima/kissvm/expressions/expression.hpp"

// Expression implementations
#include "org/minima/kissvm/expressions/boolean_expression.hpp"
#include "org/minima/kissvm/expressions/operator_expression.hpp"
#include "org/minima/kissvm/expressions/constant_expression.hpp"
#include "org/minima/kissvm/expressions/function_expression.hpp"
#include "org/minima/kissvm/expressions/global_expression.hpp"
#include "org/minima/kissvm/expressions/variable_expression.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace expressions {

using org::minima::kissvm::exceptions::MinimaParseException;
using org::minima::kissvm::tokens::LexicalTokenizer;
using org::minima::kissvm::tokens::ScriptToken;
using org::minima::kissvm::functions::MinimaFunction;
using org::minima::kissvm::values::Value;
using org::minima::kissvm::values::BooleanValue;
using org::minima::kissvm::values::NumberValue;
using org::minima::objects::base::MiniNumber;

// ============================================================================
// Entry point: Parse a flat list of tokens into an Expression tree
// ============================================================================

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getExpression(const std::vector<ScriptToken>& zTokens, int zStackDepth) {
    // Must have some tokens
    if (zTokens.empty()) {
        throw MinimaParseException("Cannot have EMPTY expression");
    }

    // Create LexicalTokenizer with the tokens
    LexicalTokenizer lt(zTokens, zStackDepth);

    // Parse the complete expression
    std::unique_ptr<org::minima::kissvm::expressions::Expression> exp = getExpression(lt);

    // Verify all tokens were consumed
    if (!lt.checkAllTokensUsed()) {
        const ScriptToken& extra = lt.getNextToken();
        throw MinimaParseException(std::string("Incorrect token number in expression @ ") + extra.getToken());
    }

    return exp;
}

// ============================================================================
// Recursion safety tracking
// ============================================================================
static thread_local int recursion_depth = 0;
const int MAX_SAFE_RECURSION = 200;

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getExpression(LexicalTokenizer& zTokens) {
    // Recursion safety check
    if (++recursion_depth > MAX_SAFE_RECURSION) {
        recursion_depth = 0;  // Reset to prevent lockout
        throw MinimaParseException("Expression parser exceeded recursion limit - possible parsing bug");
    }
    
    try {
        // Check Stack Depth
        if (zTokens.getStackDepth() > org::minima::kissvm::Contract::MAX_STACK_DEPTH) {
            throw MinimaParseException(
                "Stack too deep (MAX " + std::to_string(org::minima::kissvm::Contract::MAX_STACK_DEPTH) + ") " +
                std::to_string(zTokens.getStackDepth()));
        }

        // Top level
        std::unique_ptr<org::minima::kissvm::expressions::Expression> exp = getRelation(zTokens);

        while (zTokens.hasMoreElements()) {
            const ScriptToken& tok = zTokens.getNextToken();
            const std::string& t = tok.getToken();

            // Boolean operators - now using make_unique since inheritance is correct
            if (t == "AND") {
                auto right = getRelation(zTokens);
                exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_AND);
            } else if (t == "OR") {
                auto right = getRelation(zTokens);
                exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_OR);
            } else if (t == "XOR") {
                auto right = getRelation(zTokens);
                exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_XOR);
            } else if (t == "NAND") {
                auto right = getRelation(zTokens);
                exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_NAND);
            } else if (t == "NOR") {
                auto right = getRelation(zTokens);
                exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_NOR);
            } else if (t == "NXOR") {
                auto right = getRelation(zTokens);
                exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_NXOR);
            } else {
                // Not a boolean operator - go back
                zTokens.goBackToken();
                break;
            }
        }

        --recursion_depth;
        return exp;
    } catch (...) {
        --recursion_depth;
        throw;
    }
}

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getRelation(LexicalTokenizer& zTokens) {
    std::unique_ptr<org::minima::kissvm::expressions::Expression> exp = getLogic(zTokens);

    while (zTokens.hasMoreElements()) {
        const ScriptToken& tok = zTokens.getNextToken();
        const std::string& t = tok.getToken();

        // Comparison operators - using make_unique
        if (t == "LT") {
            auto right = getLogic(zTokens);
            exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_LT);
        } else if (t == "LTE") {
            auto right = getLogic(zTokens);
            exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_LTE);
        } else if (t == "GT") {
            auto right = getLogic(zTokens);
            exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_GT);
        } else if (t == "GTE") {
            auto right = getLogic(zTokens);
            exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_GTE);
        } else if (t == "EQ") {
            auto right = getLogic(zTokens);
            exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_EQ);
        } else if (t == "NEQ") {
            auto right = getLogic(zTokens);
            exp = std::make_unique<BooleanExpression>(std::move(exp), std::move(right), BooleanExpression::BOOLEAN_NEQ);
        } else {
            zTokens.goBackToken();
            break;
        }
    }

    return exp;
}

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getLogic(LexicalTokenizer& zTokens) {
    std::unique_ptr<org::minima::kissvm::expressions::Expression> exp = getAddSub(zTokens);

    while (zTokens.hasMoreElements()) {
        const ScriptToken& tok = zTokens.getNextToken();
        const std::string& t = tok.getToken();

        // Bitwise logic operators - using make_unique
        if (t == "&") {
            auto right = getAddSub(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_AND);
        } else if (t == "|") {
            auto right = getAddSub(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_OR);
        } else if (t == "^") {
            auto right = getAddSub(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_XOR);
        } else {
            zTokens.goBackToken();
            break;
        }
    }

    return exp;
}

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getAddSub(LexicalTokenizer& zTokens) {
    std::unique_ptr<org::minima::kissvm::expressions::Expression> exp = getMulDiv(zTokens);

    while (zTokens.hasMoreElements()) {
        const ScriptToken& tok = zTokens.getNextToken();
        const std::string& t = tok.getToken();

        // Addition/subtraction operators - using make_unique
        if (t == "+") {
            auto right = getMulDiv(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_ADD);
        } else if (t == "-") {
            auto right = getMulDiv(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_SUB);
        } else {
            zTokens.goBackToken();
            break;
        }
    }

    return exp;
}

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getMulDiv(LexicalTokenizer& zTokens) {
    std::unique_ptr<org::minima::kissvm::expressions::Expression> exp = getPrimary(zTokens);

    while (zTokens.hasMoreElements()) {
        const ScriptToken& tok = zTokens.getNextToken();
        const std::string& t = tok.getToken();

        // Multiplication/division operators - using make_unique
        if (t == "*") {
            auto right = getPrimary(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_MUL);
        } else if (t == "/") {
            auto right = getPrimary(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_DIV);
        } else if (t == "%") {
            auto right = getPrimary(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_MODULO);
        } else if (t == "<<") {
            auto right = getPrimary(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_SHIFTL);
        } else if (t == ">>") {
            auto right = getPrimary(zTokens);
            exp = std::make_unique<OperatorExpression>(std::move(exp), std::move(right), OperatorExpression::OPERATOR_SHIFTR);
        } else {
            zTokens.goBackToken();
            break;
        }
    }

    return exp;
}

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getPrimary(LexicalTokenizer& zTokens) {
    const ScriptToken& tok = zTokens.getNextToken();
    const std::string& t = tok.getToken();

    // Unary operators - using make_unique
    if (t == "NOT") {
        auto unit = getBaseUnit(zTokens);
        return std::make_unique<BooleanExpression>(std::move(unit), BooleanExpression::BOOLEAN_NOT);
    } else if (t == "NEG") {
        auto unit = getBaseUnit(zTokens);
        return std::make_unique<OperatorExpression>(std::move(unit), OperatorExpression::OPERATOR_NEG);
    } else if (t == "~") {
        auto unit = getBaseUnit(zTokens);
        return std::make_unique<OperatorExpression>(std::move(unit), OperatorExpression::OPERATOR_NOT);
    } else {
        zTokens.goBackToken();
        return getBaseUnit(zTokens);
    }
}

std::unique_ptr<org::minima::kissvm::expressions::Expression>
ExpressionParser::getBaseUnit(LexicalTokenizer& zTokens) {
    const ScriptToken& tok = zTokens.getNextToken();

    // Value literal - using make_unique
    if (tok.getTokenType() == ScriptToken::TOKEN_VALUE) {
        std::unique_ptr<Value> val = Value::getValue(tok.getToken());
        return std::make_unique<ConstantExpression>(std::move(val));

    // Negative numbers
    } else if (tok.getToken() == "-") {
        // The next token MUST be a number
        const ScriptToken& num = zTokens.getNextToken();

        // Create a negative number
        MiniNumber numv = MiniNumber(num.getToken()).mult(MiniNumber::MINUSONE());
        std::unique_ptr<Value> nv = std::make_unique<NumberValue>(numv);
        return std::make_unique<ConstantExpression>(std::move(nv));

    // Global variable - using make_unique
    } else if (tok.getTokenType() == ScriptToken::TOKEN_GLOBAL) {
        return std::make_unique<GlobalExpression>(tok.getToken());

    // User variable - using make_unique
    } else if (tok.getTokenType() == ScriptToken::TOKEN_VARIABLE) {
        return std::make_unique<VariableExpression>(tok.getToken());

    // Boolean TRUE - using make_unique
    } else if (tok.getTokenType() == ScriptToken::TOKEN_TRUE) {
        std::unique_ptr<Value> bv = std::make_unique<BooleanValue>(true);
        return std::make_unique<ConstantExpression>(std::move(bv));

    // Boolean FALSE - using make_unique
    } else if (tok.getTokenType() == ScriptToken::TOKEN_FALSE) {
        std::unique_ptr<Value> bv = std::make_unique<BooleanValue>(false);
        return std::make_unique<ConstantExpression>(std::move(bv));

    // Function call - using make_unique
    } else if (tok.getTokenType() == ScriptToken::TOKEN_FUNCTIION) {
        std::unique_ptr<MinimaFunction> func = MinimaFunction::getFunction(tok.getToken());

        // Remove the opening bracket
        const ScriptToken& bracket = zTokens.getNextToken();
        if (bracket.getTokenType() != ScriptToken::TOKEN_OPENBRACKET) {
            throw MinimaParseException("Missing opening bracket at start of function " + func->getName());
        }

        // Parse parameters until closing bracket
        int paramCount = 0;
        while (true) {
            const ScriptToken& isclosebracket = zTokens.getNextToken();

            if (isclosebracket.getTokenType() == ScriptToken::TOKEN_CLOSEBRACKET) {
                break;
            } else {
                // Go back to parse the parameter
                zTokens.goBackToken();
        
                // Increment Stack Depth
                zTokens.incrementStackDepth();
                
                // Parse the parameter as a full expression
                std::unique_ptr<org::minima::kissvm::expressions::Expression> param = getExpression(zTokens);
                func->addParameter(std::move(param));
                paramCount++;
                
                if (paramCount > org::minima::kissvm::Contract::MAX_FUNCTION_PARAMS) {
                    throw MinimaParseException(
                        "Too many function params, max " + std::to_string(org::minima::kissvm::Contract::MAX_FUNCTION_PARAMS));
                }
                
                // Decrement Stack
                zTokens.decrementStackDepth();
            }
        }

        // Check the correct number of parameters
        func->checkParamNumberCorrect();

        // Create the FunctionExpression - using make_unique
        return std::make_unique<FunctionExpression>(std::move(func));

    // Bracketed expression
    } else if (tok.getTokenType() == ScriptToken::TOKEN_OPENBRACKET) {
        // Increment Stack Depth
        zTokens.incrementStackDepth();

        // Parse the bracketed expression
        std::unique_ptr<org::minima::kissvm::expressions::Expression> exp = getExpression(zTokens);

        // Decrement Stack
        zTokens.decrementStackDepth();

        // Next token MUST be a close bracket
        const ScriptToken& closebracket = zTokens.getNextToken();
        if (closebracket.getTokenType() != ScriptToken::TOKEN_CLOSEBRACKET) {
            throw MinimaParseException(std::string("Missing close bracket. Found : ") + closebracket.getToken());
        }

        return exp;

    } else {
        throw MinimaParseException(
            "Incorrect Token in script " + tok.getToken() + " @ " + std::to_string(zTokens.getCurrentPosition()));
    }
}

} // namespace expressions
} // namespace kissvm
} // namespace minima
} // namespace org