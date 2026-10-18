#include "org/minima/kissvm/statements/statement_parser.hpp"

#include <algorithm>
#include <cctype>

// Project headers (full includes required in .cpp per Rule 10)
#include "org/minima/kissvm/contract.hpp"
#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"
#include "org/minima/kissvm/expressions/expression.hpp"
#include "org/minima/kissvm/expressions/expression_parser.hpp"
#include "org/minima/kissvm/expressions/constant_expression.hpp"
#include "org/minima/kissvm/tokens/lexical_tokenizer.hpp"
#include "org/minima/kissvm/tokens/script_token.hpp"
#include "org/minima/kissvm/values/boolean_value.hpp"

#include "org/minima/kissvm/statements/statement.hpp"
#include "org/minima/kissvm/statements/statement_block.hpp"

#include "org/minima/kissvm/statements/commands/a_s_s_e_r_tstatement.hpp"
#include "org/minima/kissvm/statements/commands/e_x_e_cstatement.hpp"
#include "org/minima/kissvm/statements/commands/i_fstatement.hpp"
#include "org/minima/kissvm/statements/commands/l_e_tstatement.hpp"
#include "org/minima/kissvm/statements/commands/m_a_s_tstatement.hpp"
#include "org/minima/kissvm/statements/commands/r_e_t_u_r_nstatement.hpp"
#include "org/minima/kissvm/statements/commands/w_h_i_l_estatement.hpp"

#ifdef _WIN32
// No OS-specific behavior required. Placeholder to satisfy cross-platform note.
#endif

namespace org {
namespace minima {
namespace kissvm {
namespace statements {

using org::minima::kissvm::exceptions::MinimaParseException;
using org::minima::kissvm::expressions::ExpressionParser;
using org::minima::kissvm::tokens::LexicalTokenizer;
using org::minima::kissvm::tokens::ScriptToken;

namespace {
    bool equalsIgnoreCase(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned char ca = static_cast<unsigned char>(a[i]);
            unsigned char cb = static_cast<unsigned char>(b[i]);
            if (std::tolower(ca) != std::tolower(cb)) return false;
        }
        return true;
    }
}

std::unique_ptr<org::minima::kissvm::statements::StatementBlock>
StatementParser::parseTokens(const std::vector<ScriptToken>& zTokens, int zStackDepth) {
    using org::minima::kissvm::Contract;
    using org::minima::kissvm::expressions::Expression;
    using org::minima::kissvm::statements::Statement;
    using org::minima::kissvm::statements::StatementBlock;
    using org::minima::kissvm::statements::commands::ASSERTstatement;
    using org::minima::kissvm::statements::commands::EXECstatement;
    using org::minima::kissvm::statements::commands::IFstatement;
    using org::minima::kissvm::statements::commands::LETstatement;
    using org::minima::kissvm::statements::commands::MASTstatement;
    using org::minima::kissvm::statements::commands::RETURNstatement;
    using org::minima::kissvm::statements::commands::WHILEstatement;

    std::vector<std::unique_ptr<Statement>> stats;

    // The current stack depth
    int currentStackDepth = zStackDepth + 1;

    // Check stack depth
    if (currentStackDepth > Contract::MAX_STACK_DEPTH) {
        throw MinimaParseException(
            "Stack too deep (MAX " + std::to_string(Contract::MAX_STACK_DEPTH) + ") " + std::to_string(currentStackDepth)
        );
    }

    // Iterate through tokens
    int currentPosition = 0;
    int totaltokens = static_cast<int>(zTokens.size());

    while (currentPosition < totaltokens) {
        // Get the current token
        const ScriptToken& tok = zTokens.at(currentPosition++);
        std::string token = tok.getToken();
        int type = tok.getTokenType();

        if (type != ScriptToken::TOKEN_COMMAND) {
            throw MinimaParseException("Invalid Token where there should be a COMMMAND - " + token);
        }

        // Cycle through commands
        if (equalsIgnoreCase(token, "LET")) {
            // The next token is either the variable name or an array position..
            const ScriptToken& var0 = zTokens.at(currentPosition++);
            int vartype = var0.getTokenType();

            // Is it a simple variable LET or an ARRAY set LET
            if (vartype == ScriptToken::TOKEN_OPENBRACKET) {
                // Get the tokens to the equals sign
                std::vector<ScriptToken> arraypos = getTokensToNextEquals(zTokens, currentPosition);
                currentPosition += static_cast<int>(arraypos.size());

                // Check the last token is a close bracket
                int arrsize = static_cast<int>(arraypos.size());
                if (arrsize == 0) {
                    throw MinimaParseException("Incorrect LET statement, missing ) .. )");
                }
                const ScriptToken& var = arraypos.at(arrsize - 1);
                if (var.getTokenType() != ScriptToken::TOKEN_CLOSEBRACKET) {
                    throw MinimaParseException("Incorrect LET statement, missing ) .. " + var.getToken() + ")");
                }

                // The next token is always =
                const ScriptToken& eq = zTokens.at(currentPosition++);
                if (eq.getTokenType() != ScriptToken::TOKEN_OPERATOR && eq.getToken() != "=") {
                    throw MinimaParseException("Incorrect LET statement, missing = (.." + eq.getToken() + ")");
                }

                // Remove the last token..
                arraypos.pop_back();

                // Check is a valid non-empty expression
                if (arraypos.empty()) {
                    throw MinimaParseException("Incorrect LET statement, EMPTY ARRAY POS @ " + std::to_string(currentPosition));
                }

                // Create a Lexical Tokenizer.. there may be multiple expressions..
                std::vector<ScriptToken> tokensCopy = arraypos;
                LexicalTokenizer lt(tokensCopy, currentStackDepth);

                std::vector<std::unique_ptr<Expression>> exps;
                while (!lt.checkAllTokensUsed()) {
                    // Now get each of the expressions
                    std::unique_ptr<Expression> letexp = ExpressionParser::getExpression(lt);
                    // Add it to our list for the LET statement
                    exps.emplace_back(std::move(letexp));
                }

                // Now find the next Command, and everything in between is the expression
                std::vector<ScriptToken> lettokens = getTokensToNextCommand(zTokens, currentPosition);
                currentPosition += static_cast<int>(lettokens.size());

                // Now create an expression from those tokens..
                std::unique_ptr<Expression> exp = ExpressionParser::getExpression(lettokens, currentStackDepth);

                // And finally create the LET statement..
                stats.emplace_back(std::make_unique<LETstatement>(std::move(exps), std::move(exp)));

            } else if (vartype == ScriptToken::TOKEN_VARIABLE) {
                // The Variable name
                std::string varname = var0.getToken();

                // The next token is always =
                const ScriptToken& eq = zTokens.at(currentPosition++);
                if (eq.getToken() != "=") {
                    throw MinimaParseException("Incorrect LET statement, missing = (.." + eq.getToken() + ")");
                }

                // Now find the next Command, and everything in between is the expression
                std::vector<ScriptToken> lettokens = getTokensToNextCommand(zTokens, currentPosition);
                currentPosition += static_cast<int>(lettokens.size());

                // Now create an expression from those tokens..
                std::unique_ptr<Expression> exp = ExpressionParser::getExpression(lettokens, currentStackDepth);

                // And finally create the LET statement..
                stats.emplace_back(std::make_unique<LETstatement>(varname, std::move(exp)));

            } else {
                throw MinimaParseException("Not a variable or array after LET (.." + var0.getToken() + ")");
            }

        } else if (equalsIgnoreCase(token, "EXEC")) {
            // Now find the next Command, and everything in between is the expression
            std::vector<ScriptToken> exectokens = getTokensToNextCommand(zTokens, currentPosition);
            currentPosition += static_cast<int>(exectokens.size());

            // Now create an expression from those tokens..
            auto exp = ExpressionParser::getExpression(exectokens, currentStackDepth);

            // And finally create the EXEC statement..
            stats.emplace_back(std::make_unique<EXECstatement>(std::move(exp)));

        } else if (equalsIgnoreCase(token, "MAST")) {
            // Now find the next Command, and everything in between is the expression
            std::vector<ScriptToken> masttokens = getTokensToNextCommand(zTokens, currentPosition);
            currentPosition += static_cast<int>(masttokens.size());

            // Now create an expression from those tokens..
            auto exp = ExpressionParser::getExpression(masttokens, currentStackDepth);

            // And finally create the MAST statement..
            stats.emplace_back(std::make_unique<MASTstatement>(std::move(exp)));

        } else if (equalsIgnoreCase(token, "IF")) {
            // An IFX
            auto ifsx = std::make_unique<IFstatement>();

            // Get the IF Conditional
            std::vector<ScriptToken> conditiontokens = getTokensToRequiredCommand(zTokens, currentPosition, "THEN");

            // Now create an expression from those tokens..
            auto IFcondition = ExpressionParser::getExpression(conditiontokens, currentStackDepth);

            // Increments
            currentPosition += static_cast<int>(conditiontokens.size()) + 1;

            // Now get the Expression..
            std::vector<ScriptToken> actiontokens = getElseOrElseIfOrEndIF(zTokens, currentPosition, true);

            // Increment
            currentPosition += static_cast<int>(actiontokens.size());

            // Is it the ENDIF or the ELSE
            std::string nexttok = actiontokens.back().getToken();

            // Remove the final ENDIF/ELSE/ELSEIF marker
            actiontokens.pop_back();

            // And convert that block of Tokens into a block of code..
            std::unique_ptr<StatementBlock> IFaction = StatementParser::parseTokens(actiontokens, currentStackDepth);

            // Add what we know to the IF statement..
            ifsx->addCondition(std::move(IFcondition), std::move(IFaction));

            // Is it ELSE or END
            while (nexttok != "ENDIF") {
                std::unique_ptr<Expression>     ELSEcondition;
                std::unique_ptr<StatementBlock> ELSEaction;

                if (nexttok == "ELSE") {
                    // ELSE is default TRUE - build it via ExpressionParser from a TRUE token
                    std::vector<ScriptToken> truetoks;
                    truetoks.emplace_back(ScriptToken::TOKEN_TRUE, "TRUE");
                    ELSEcondition = ExpressionParser::getExpression(truetoks, currentStackDepth);

                } else if (nexttok == "ELSEIF") {
                    // It's ELSEIF
                    conditiontokens = getTokensToRequiredCommand(zTokens, currentPosition, "THEN");

                    // Create an Expression..
                    ELSEcondition = ExpressionParser::getExpression(conditiontokens, currentStackDepth);

                    // Increments
                    currentPosition += static_cast<int>(conditiontokens.size()) + 1;
                } else {
                    // Incorrect IF statement
                    throw MinimaParseException("MISSING ELSE or ELSEIF in IF Statement");
                }

                // Now get the Action..
                actiontokens = getElseOrElseIfOrEndIF(zTokens, currentPosition, true);

                // Increment
                currentPosition += static_cast<int>(actiontokens.size());

                // Is it the ENDIF or the ELSE/ELSEIF
                nexttok = actiontokens.back().getToken();

                // Remove the final marker (ENDIF/ELSE/ELSEIF)
                actiontokens.pop_back();

                // And convert that block of Tokens into a block of code..
                ELSEaction = StatementParser::parseTokens(actiontokens, currentStackDepth);

                // Add what we know to the IF statement..
                ifsx->addCondition(std::move(ELSEcondition), std::move(ELSEaction));
            }

            // Add IF to statements
            stats.emplace_back(std::move(ifsx));

        } else if (equalsIgnoreCase(token, "WHILE")) {
            // Get the WHILE Conditional - stop at the next DO
            std::vector<ScriptToken> conditiontokens = getTokensToRequiredCommand(zTokens, currentPosition, "DO");

            // Now create an expression from those tokens..
            auto WHILEcondition = ExpressionParser::getExpression(conditiontokens, currentStackDepth);

            // Increments
            currentPosition += static_cast<int>(conditiontokens.size()) + 1;

            // Now get the Expression..
            std::vector<ScriptToken> actiontokens = getEndWHILE(zTokens, currentPosition);

            // Increment
            currentPosition += static_cast<int>(actiontokens.size());

            // Remove the final ENDWHILE
            actiontokens.pop_back();

            // And convert that block of Tokens into a block of code..
            auto WHILEaction = StatementParser::parseTokens(actiontokens, currentStackDepth);

            // Create a WHILE statement
            stats.emplace_back(std::make_unique<WHILEstatement>(std::move(WHILEcondition), std::move(WHILEaction)));

        } else if (equalsIgnoreCase(token, "ASSERT")) {
            // The Next tokens are the Expression..
            std::vector<ScriptToken> returntokens = getTokensToNextCommand(zTokens, currentPosition);
            currentPosition += static_cast<int>(returntokens.size());

            // Now create an expression from those tokens..
            auto exp = ExpressionParser::getExpression(returntokens, currentStackDepth);

            // Create a new ASSERT statement
            stats.emplace_back(std::make_unique<ASSERTstatement>(std::move(exp)));

        } else if (equalsIgnoreCase(token, "RETURN")) {
            // The Next tokens are the Expression..
            std::vector<ScriptToken> returntokens = getTokensToNextCommand(zTokens, currentPosition);
            currentPosition += static_cast<int>(returntokens.size());

            // Now create an expression from those tokens..
            auto exp = ExpressionParser::getExpression(returntokens, currentStackDepth);

            // Create a new RETURN statement
            stats.emplace_back(std::make_unique<RETURNstatement>(std::move(exp)));

        } else {
            throw MinimaParseException("Invalid Token where there should be a Command - " + token);
        }
    }

    // Return the constructed StatementBlock
    return std::make_unique<StatementBlock>(std::move(stats));
}

std::vector<ScriptToken>
StatementParser::getElseOrElseIfOrEndIF(const std::vector<ScriptToken>& zTokens,
                                        int zCurrentPosition,
                                        bool zElseAlso) {
    std::vector<ScriptToken> rettokens;

    int currentpos = zCurrentPosition;
    int total = static_cast<int>(zTokens.size());

    // Cycle through the tokens..
    while (currentpos < total) {
        const ScriptToken& tok = zTokens.at(currentpos);

        if (tok.getTokenType() == ScriptToken::TOKEN_COMMAND && tok.getToken() == "ENDIF") {
            // We've found the end to the current depth IF
            rettokens.push_back(tok);
            break;

        } else if (zElseAlso && (tok.getTokenType() == ScriptToken::TOKEN_COMMAND && tok.getToken() == "ELSEIF")) {
            // We've found the end to the current depth IF
            rettokens.push_back(tok);
            break;

        } else if (zElseAlso && (tok.getTokenType() == ScriptToken::TOKEN_COMMAND && tok.getToken() == "ELSE")) {
            // We've found the end to the current depth IF
            rettokens.push_back(tok);
            break;

        } else if (tok.getTokenType() == ScriptToken::TOKEN_COMMAND && tok.getToken() == "IF") {
            // Add it..
            rettokens.push_back(tok);
            currentpos++;

            // Go down One Level
            std::vector<ScriptToken> toks = getElseOrElseIfOrEndIF(zTokens, currentpos, false);

            rettokens.insert(rettokens.end(), toks.begin(), toks.end());
            currentpos += static_cast<int>(toks.size());

        } else {
            // Just add it to the list
            rettokens.push_back(tok);
            currentpos++;
        }
    }

    return rettokens;
}

std::vector<ScriptToken>
StatementParser::getEndWHILE(const std::vector<ScriptToken>& zTokens, int zCurrentPosition) {
    std::vector<ScriptToken> rettokens;

    int currentpos = zCurrentPosition;
    int total = static_cast<int>(zTokens.size());

    // Cycle through the tokens..
    while (currentpos < total) {
        const ScriptToken& tok = zTokens.at(currentpos);

        if (tok.getTokenType() == ScriptToken::TOKEN_COMMAND && tok.getToken() == "ENDWHILE") {
            // We've found the end to the current depth WHILE
            rettokens.push_back(tok);
            break;

        } else if (tok.getTokenType() == ScriptToken::TOKEN_COMMAND && tok.getToken() == "WHILE") {
            // Add it..
            rettokens.push_back(tok);
            currentpos++;

            // Go down One Level
            std::vector<ScriptToken> toks = getEndWHILE(zTokens, currentpos);

            rettokens.insert(rettokens.end(), toks.begin(), toks.end());
            currentpos += static_cast<int>(toks.size());

        } else {
            // Just add it to the list
            rettokens.push_back(tok);
            currentpos++;
        }
    }

    return rettokens;
}

std::vector<ScriptToken>
StatementParser::getTokensToNextCommand(const std::vector<ScriptToken>& zTokens, int zCurrentPosition) {
    std::vector<ScriptToken> rettokens;

    int ret = zCurrentPosition;
    int total = static_cast<int>(zTokens.size());
    while (ret < total) {
        const ScriptToken& tok = zTokens.at(ret);
        if (tok.getTokenType() == ScriptToken::TOKEN_COMMAND) {
            break;
        } else {
            rettokens.push_back(tok);
        }
        ret++;
    }

    return rettokens;
}

std::vector<ScriptToken>
StatementParser::getTokensToRequiredCommand(const std::vector<ScriptToken>& zTokens,
                                            int zCurrentPosition,
                                            const std::string& zRequiredToken) {
    std::vector<ScriptToken> rettokens;

    int ret = zCurrentPosition;
    int total = static_cast<int>(zTokens.size());
    bool found = false;
    while (ret < total) {
        const ScriptToken& tok = zTokens.at(ret);
        if (tok.getToken() == zRequiredToken) {
            found = true;
            break;
        } else {
            rettokens.push_back(tok);
        }
        ret++;
    }

    // Did we find it..
    if (!found) {
        throw MinimaParseException("Could not find required token : " + zRequiredToken);
    }

    return rettokens;
}

std::vector<ScriptToken>
StatementParser::getTokensToNextEquals(const std::vector<ScriptToken>& zTokens, int zCurrentPosition) {
    std::vector<ScriptToken> rettokens;

    int ret = zCurrentPosition;
    int total = static_cast<int>(zTokens.size());
    while (ret < total) {
        const ScriptToken& tok = zTokens.at(ret);
        if (tok.getTokenType() == ScriptToken::TOKEN_OPERATOR && tok.getToken() == "=") {
            break;
        } else {
            rettokens.push_back(tok);
        }
        ret++;
    }

    return rettokens;
}

} // namespace statements
} // namespace kissvm
} // namespace minima
} // namespace org